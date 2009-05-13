/*-
 * client.c
 * This file is part of Methanol
 *
 * Copyright (c) 2009, Emil Romanus <sdac@bithack.se>
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>
#include <mysql/mysql.h>

#include "slave.h"
#include "client.h"
#include "nolp.h"
#include "hook.h"

void nol_s_client_main(struct client *this, int sock);
static int get_and_send_url(struct client *cl);

static void client_event(EV_P_ ev_io *w, int revents);
static void thr_signal(EV_P_ ev_async *w, int revents);
static void timer_reached(EV_P_ ev_timer *w, int revents);

static int on_status(nolp_t *no, char *buf, int size);
static int on_url(nolp_t *no, char *buf, int size);
static int on_target(nolp_t *no, char *buf, int size);
static int on_count(nolp_t *no, char *buf, int size);
static int on_target_recv(nolp_t *no, char *buf, int size);

static int update_ft_attr(struct client *cl, char *attr, int attr_len, char *val, int val_len);
static int send_config(int sock);

char *nol_s_str_filter_quote(char *s, int size);
char *nol_s_str_filter_name(char *s, int size);

/* nolp commands, client -> slave */
struct nolp_fn client_commands[] = {
    {"STATUS", &on_status},
    {"URL", &on_url},
    {"TARGET", &on_target},
    {"COUNT", &on_count},
    {0}
};

/** 
 * Create a new client with a random login-token
 *
 * the token is a 40-byte string representation
 * of a SHA1 hash generated by an SQL query on
 * some random values and the IP-address 
 * concatenated.
 *
 * addr should be the clients IP-address
 **/
struct client*
nol_s_client_create(const char *addr, const char *user)
{
    struct client* cl;
    time_t now;
    int    a, x;
    char  *p;
    char   q[128];
    int    qlen;

    MYSQL_RES *res;
    MYSQL_ROW *row;

    if ((cl = calloc(1, sizeof(struct client)))) {
        if ((cl->addr.s_addr = inet_addr(addr)) == (in_addr_t)-1
                || !(cl->user = strdup(user))) {
            syslog(LOG_ERR, "invalid address/user");
            free(cl);
            return 0;
        }
        qlen = sprintf(q,
                "SELECT SHA1(CONCAT(CONCAT(CONCAT('%s', NOW()), RAND()), RAND()))",
                addr);
        if (mysql_real_query(srv.mysql, q, qlen) != 0
                || !(res = mysql_store_result(srv.mysql))
                || !(mysql_num_rows(res))
                || !(row = mysql_fetch_row(res))
                ) {
            free(cl);
            return 0;
        }
        if (row[0])
            memcpy(cl->token, row[0], 40);
        else
            return 0;
        cl->token[40] = (char)0;

        mysql_free_result(res);

        /** 
         * add the client to the _client table which will give us an 
         * integer identifier to be used when adding URLs to the log
         * and save target urls
         **/
        qlen = sprintf(q,
                "INSERT INTO nol_client (token) VALUES ('%.40s');",
                cl->token);
        if (mysql_real_query(srv.mysql, q, qlen) != 0) {
            free(cl);
            return 0;
        }
        cl->id = (long)mysql_insert_id(srv.mysql);
        cl->session_id = 0;

        syslog(LOG_INFO,"new client %d:'%.6s...' from %s",
                (int)cl->id, cl->token, addr);
    }

    return cl;
}

void
nol_s_client_free(struct client *cl)
{
    free(cl->user);
    free(cl);
}

void *
nol_s_client_init(void *in)
{
    int sock = (int)in;
    int n;
    int x;
    char *buf;
    struct client *this = 0;

    do {
        if (!(buf = malloc(256)))
            break;

        /* first authorize this client connection */
        if ((n = sock_getline(sock, buf, 255)) <= 0)
            break;

        if (n != 6+TOKEN_SIZE || memcmp(buf, "AUTH ", 5) != 0)
            break;

        pthread_mutex_lock(&srv.pending_lk);
        for (x=0; x<srv.num_pending; x++) {
            if (memcmp(srv.pending[x]->token, buf+5, TOKEN_SIZE) == 0) {
                this = srv.pending[x];
                if (x != srv.num_pending-1)
                    srv.pending[x] = srv.pending[srv.num_pending-1];
                srv.num_pending --;
                break;
            }
        }
        pthread_mutex_unlock(&srv.pending_lk);

        free(buf);
        buf = 0;

        if (!this) {
            send(sock, "200 Denied\n", 11, 0);
            break;
        }

        /* enter main loop */
        nol_s_client_main(this, sock);
    } while (0);

    if (buf)
        free(buf);
    close(sock);

    /* remove this client from the client list, 
     * if it's been added to it earlier */
    if (this) {
        /* if a session was started but not stopped, we'll mark it as interrupted 
         * right now */
        if (this->session_id) {
            char *q = malloc(128);
            int   sz;
            sz = sprintf(q, "UPDATE `nol_session` SET state='interrupted', latest=NOW() WHERE id=%ld",
                    this->session_id);
            mysql_real_query(this->mysql, q, sz);
            free(q);
        }
        mysql_close(this->mysql);

        pthread_mutex_lock(&srv.clients_lk);
        for (x=0; x<srv.num_clients; x++) {
            if (srv.clients[x] == this) {
                if (x != srv.num_clients-1) {
                    srv.clients[x] = srv.clients[srv.num_clients-1];
                }
                if (srv.num_clients == 1) {
                    free(srv.clients);
                    srv.clients = 0;
                } else if (!(srv.clients = realloc(srv.clients, (srv.num_clients-1)*sizeof(struct client*))))
                        abort();
                srv.num_clients--;
                break;
            }
        }
        pthread_mutex_unlock(&srv.clients_lk);
        ev_async_send(EV_DEFAULT_ &srv.client_status);
        nol_s_client_free(this);

        ev_async_send(EV_DEFAULT_ &srv.client_status);
    }

    return 0;
}

void
nol_s_client_main(struct client *this, int sock)
{
    struct ev_loop *loop;
    ev_io           io;

    if (!(this->no = nolp_create(&client_commands, sock)))
        return;
    if (!(loop = this->loop = ev_loop_new(EVFLAG_AUTO)))
        return;
    if (!(this->mysql = nol_s_dup_mysql_conn()))
        return;

    ((nolp_t *)(this->no))->private = this;

    ev_io_init(&io, &client_event, sock, EV_READ);
    ev_async_init(&this->async, &thr_signal);
    this->async.data = this;

    /* move 'this' into the global list of clients
     * at srv.clients, this allows the main thread 
     * to signal this thread and retrieve data
     * from the 'this' structure */
    pthread_mutex_lock(&srv.clients_lk);
    if (!(srv.clients = realloc(srv.clients, (srv.num_clients+1)*sizeof(struct client*)))) {
        syslog(LOG_ERR, "out of mem");
        abort();
    }
    srv.clients[srv.num_clients] = this;
    srv.num_clients++;
    pthread_mutex_unlock(&srv.clients_lk);

    io.data = this->no;
    ev_io_start(loop, &io);
    ev_async_start(loop, &this->async);
    
    /* notify the main thread that we chagned the client list,
     * the slave in turn will update the master with the new 
     * list */
    this->running = 0;
    ev_async_send(EV_DEFAULT_ &srv.client_status);

    /* notify the login success to the client, and then send
     * the configuration file */
    send(sock, "100 OK\n", 7, 0);
    if (send_config(sock) == 0) {
        ev_loop(loop, 0);
    }
    ev_io_stop(loop, &io);
    ev_async_stop(loop, &this->async);
    ev_loop_destroy(loop);
    syslog(LOG_INFO, "client '%.7s...' disconnected", this->token);
}

/* send the active configuration as received from the
 * master to the client, this must be done be anything else
 * is done */
static int
send_config(int sock)
{
    char out[64];
    int  len;
    len = sprintf(out, "CONFIG %d\n", srv.config_sz);
    if (send(sock, out, len, 0) <= 0)
        return -1;
    if (send(sock, srv.config_buf, srv.config_sz, 0) != srv.config_sz)
        return -1;

    return 0;
}

/** 
 * called when data is available for 
 * reading on the client socket
 **/
static void
client_event(EV_P_ ev_io *w, int revents)
{
    if (nolp_recv((nolp_t *)w->data) != 0)
        ev_unloop(EV_A_ EVUNLOOP_ONE);
}

/** 
 * Receive signal from main thread. The main thread will set the
 * variable client->msg and then invoke this signal.
 **/
static void
thr_signal(EV_P_ ev_async *w, int revents)
{
    struct client *cl = (struct client*)w->data;
    switch (cl->msg) {
        case NOL_CLIENT_MSG_KILL:
            /* stuff such as marking the session as INTERRUPTED will
             * be taken care of in nol_s_client_main() after the 
             * call to ev_loop */
            ev_unloop(EV_A_ EVUNLOOP_ONE);
            break;
    }
}

/* see on_status() for why the timer is used */
static void
timer_reached(EV_P_ ev_timer *w, int revents)
{
    char *p;
    /* return 0 if we successfully found a URL and sent
     * it to the client */
        syslog(LOG_DEBUG, "timer reached, cl '%.7s'", ((struct client*)w->data)->token);
    switch (get_and_send_url((struct client*)w->data)) {
        case -1:
            p = mysql_error(((struct client*)w->data)->mysql);
            syslog(LOG_ERR, "URL get and send failed: %s", p?(*p?p:"error"):"error");
            ev_timer_stop(EV_A_ w);
            ev_unloop(EV_A_ EVUNLOOP_ONE);
        case 0:
            ev_timer_stop(EV_A_ w);
            return;
        case 1:
            ev_timer_again(loop, w);
    }
}

#define Q_GET_NEW_URL \
    "SELECT id, crawler, input FROM nol_added WHERE `date` <= NOW()"\
    "ORDER BY `date` DESC LIMIT 0,1;"

/** 
 * get_and_send_url()
 *
 * query the database for any new URL, if found
 * then send it to the client. Returns 0 if a 
 * URL was found and sent to the client, 1 if no
 * URL was found, or -1 if an error occured
 **/
static int
get_and_send_url(struct client *cl)
{
    MYSQL_RES *res;
    MYSQL_ROW row;
    int   sz;
    char *url;
    char *buf;
    int   id;
    int   ret;
    unsigned long *lengths;

    if (mysql_query(cl->mysql, "LOCK TABLES nol_added WRITE") != 0) {
        syslog(LOG_ERR, "could not lock url table!");
        return -1;
    }

    if (mysql_real_query(cl->mysql, Q_GET_NEW_URL, sizeof(Q_GET_NEW_URL)-1) != 0)
        goto fail_do_unlock;
    if (!(res = mysql_store_result(cl->mysql)))
        goto fail_do_unlock;
    if (mysql_num_rows(res)) {
        if (!(row = mysql_fetch_row(res))
                || !(lengths = mysql_fetch_lengths(res))) {
            mysql_free_result(res);
            goto fail_do_unlock;
        }
        id = atoi(row[0]);
        if ((sz = lengths[1]+lengths[2]+1) < 256)
            sz = 256;
        if (!(buf = malloc(sz+10))) {
            mysql_free_result(res);
            goto fail_do_unlock;
        }
        sz = sprintf(buf, 
                "UPDATE nol_added "
                "SET date = DATE_ADD(NOW(), INTERVAL 28 DAY) "
                "WHERE id=%d LIMIT 1;",
                id);
        if (mysql_real_query(cl->mysql, buf, sz) != 0) {
            syslog(LOG_ERR, "mysql error when updating nol_added: %s",
                             mysql_error(cl->mysql));
            mysql_free_result(res);
            mysql_query(cl->mysql, "UNLOCK TABLES");
            return -1;
        }
        mysql_query(cl->mysql, "UNLOCK TABLES");
        sz = sprintf(buf, "START %s %s\n", row[1], row[2]);
#ifdef DEBUG
        syslog(LOG_DEBUG, "sending url '%.*s' to client '%.7s...'",
                (int)lengths[2], row[2], cl->token);
#endif
        send(((nolp_t *)(cl->no))->fd, buf, sz, 0);
        /* create a session for this client, the session will last
         * until the client is out of URLs and sends a STATUS 0 
         * message */
        sz = sprintf(buf,
                "INSERT INTO `nol_session` (added_id, client_id, date, latest)"
                "VALUES (%d, '%ld', NOW(), NOW());",
                id, cl->id
                );
        if (mysql_real_query(cl->mysql, buf, sz) == 0) {
            cl->session_id = mysql_insert_id(cl->mysql);
#ifdef DEBUG
            syslog(LOG_DEBUG, "client '%.7s...' now running session %ld",
                    cl->token, cl->session_id);
#endif
            ret = 0;
        } else
            ret = -1;

        mysql_free_result(res);
        free(buf);
    } else {
        mysql_query(cl->mysql, "UNLOCK TABLES");
        ret = 1;
    }

    return ret;

fail_do_unlock:
    syslog(LOG_ERR, "url get and send failed: %s", mysql_error(cl->mysql));
    mysql_query(cl->mysql, "UNLOCK TABLES");
    return -1;
}

/** 
 * STATUS command
 **/
static int
on_status(nolp_t *no, char *buf, int size)
{
    struct client *cl;
    int status;
    char q[128];
    char *p;

    cl = (struct client*)no->private;
    cl->running = status = atoi(buf);

#ifdef DEBUG
    syslog(LOG_DEBUG, "client '%.7s...' set status to %d", 
            cl->token, status);
#endif

    /** 
     * if status goes to 0, we'll try to find a new
     * URL for this client. If no URL can be found, we'll
     * start a timer that periodically checks for new 
     * URLs.
     **/
    if (status == 0) {
        if (cl->session_id) {
#ifdef DEBUG
            syslog(LOG_DEBUG, "session %ld finished", cl->session_id);
#endif
            sprintf(q, "UPDATE `nol_session` SET state='hook', latest=NOW() WHERE id=%ld",
                    cl->session_id);
            mysql_query(cl->mysql, q);

            nol_s_hook_invoke(HOOK_SESSION_COMPLETE);
            sprintf(q, "UPDATE `nol_session` SET state='done', latest=NOW() WHERE id=%ld",
                    cl->session_id);
            mysql_query(cl->mysql, q);
        }

        if (!ev_is_active(&cl->timer)) {
            switch (get_and_send_url(cl)) {
                case -1:
                    p = mysql_error(cl->mysql);
                    syslog(LOG_ERR, "URL get and send failed: %s",
                            p?(*p?p:"error"):"error");
                    ev_unloop(cl->loop, EVUNLOOP_ONE);
                case 0:
                    return 0;
                case 1:
                    ev_timer_init(&cl->timer, &timer_reached, 5.f, .0f);
                    cl->timer.data = cl;
                    cl->timer.repeat = 5.f;
                    ev_timer_start(cl->loop, &cl->timer);
            }
        }
    }

    ev_async_send(EV_DEFAULT_ &srv.client_status);
    return 0;
}

#define Q_URL_1 \
                "INSERT INTO nol_url (url, hash, date)" \
                "VALUES ('"
#define Q_URL_2 \
                "', SHA1(url), NOW())" \
                "ON DUPLICATE KEY UPDATE date=NOW()"
/** 
 * The URL command simply tells us what URL
 * the client is crawling
 **/
static int
on_url(nolp_t *no, char *buf, int size)
{
    struct client *cl;
    char          *q;
    int            x;
    int            sz;
    int            ret = 0;
    cl = (struct client*)no->private;

    if (!cl->running || !cl->session_id)
        return -1;

    sz = size+sizeof(Q_URL_1 Q_URL_2);
    if (!(q = malloc(sz)))
        return -1;

    memcpy(q, Q_URL_1, sizeof(Q_URL_1)-1);

    /* copy the URL, but replace possible '\''
     * with '_' to avoid sql injections */
    for (x=0; x<size; x++) {
        char *p = (q+sizeof(Q_URL_1)-1)+x;
        if (buf[x] == '\'')
            *p = '_';
        else
            *p = buf[x];
    }
    memcpy(q+sizeof(Q_URL_1)-1+size,
           Q_URL_2, sizeof(Q_URL_2));
    if (mysql_real_query(cl->mysql, q, sz) != 0) {
        syslog(LOG_ERR, "updating nol_url table failed: %s", mysql_error(cl->mysql));
        ret = -1;
    }
    free(q);

    return ret;
}

#define Q_ADD_TARGET_1 "INSERT INTO ft_"
#define Q_ADD_TARGET_2 " VALUES ()"

/** 
 * Syntax for TARGET:
 * 
 * TARGET <parent-url> <url> <filetype> <size>\n
 * ... attributes ...
 *
 * Currently, parent-url is unused and will be 0.
 **/
static int
on_target(nolp_t *no, char *buf,
          int size)
{
    struct client *cl;
    char *p = buf;
    char *e = buf+size;
    char *url;
    char *filetype;
    char q[size+96];
    int  len;
    int  x;

    cl = ((struct client *)no->private);
    if (!cl->running || !cl->session_id) {
        /* client shouldnt send any target info if 
         * a session hasnt been started, disconnect the
         * client */
        return -1;
    }

    if (!(p = memchr(p, ' ', e-p)))
        return -1;
    url = p+1;
    p++;
    if (!(p = memchr(p, ' ', e-p)))
        return -1;
    filetype = p+1;
    p++;
    if (!(p = memchr(p, ' ', e-p)))
        return -1;

    if (p-filetype > 63)
        len = 63;
    else
        len = p-filetype;

    /* replace all chars thats not A-Za-z0-9_ with '_' */
    for (x=0; x<len; x++)
        *(cl->filetype_name+x) = 
            (!isalnum(*(filetype+x))?'_':*(filetype+x));
    *(cl->filetype_name+x) = '\0';
    *p = '\0';
    *(filetype-1) = '\0';
    for (x=0; x<filetype-1-url; x++)
        if (*(url+x) == '\'')
            *(url+x) = '_';

    len = sprintf(q,
            "INSERT INTO ft_%.64s (url_hash, date) VALUES (SHA1('%s'), NOW()) "
            "ON DUPLICATE KEY UPDATE date=NOW()",
            cl->filetype_name,
            url);

    if (mysql_real_query(cl->mysql, q, len) != 0) {
        syslog(LOG_ERR, "saving target failed: %s", mysql_error(cl->mysql));
        return -1;
    }

    cl->target_id = mysql_insert_id(cl->mysql);

    /* link this target with the current session */
    len = sprintf(q,
            "INSERT IGNORE INTO nol_session_rel (session_id, filetype, target_id) "
            "VALUES (%ld, '%.64s', %ld)",
            cl->session_id, cl->filetype_name, cl->target_id);

    if (mysql_real_query(cl->mysql, q, len) != 0) {
        syslog(LOG_ERR, "insert to session_rel failed: %s",
                mysql_error(cl->mysql));
        return -1;
    }

#ifdef DEBUG
    syslog(LOG_DEBUG,
            "start receiving attributes for '%s' of type '%s', sess #%ld",
            url, cl->filetype_name, cl->session_id);
#endif

    nolp_expect(no, atoi(p+1), &on_target_recv);
    return 0;
}
/**
 * Command received from the client when a target has been 
 * found, 'buf' should contain a list of attributes to 
 * save along with this target file type.
 * 
 * Each attribute will look like this:
 * 
 * <name> <size> <data>
 **/
static int
on_target_recv(nolp_t *no, char *buf,
               int size)
{
    char *p = buf,
         *e = buf+size,
         *attr,
         *value;
    struct client *cl;
    int value_len, attr_len;
    cl = (struct client *)no->private;

    while (p<e) {
        attr = p;
        if (!(p = memchr(p, ' ', e-p)))
            return -1;
        attr_len = p-attr;
        p++;
        value_len = atoi(p);
        if (!(p = memchr(p, ' ', e-p)))
            return -1;
        p++;
        value = p;
        if (p+value_len > e)
            return -1;

        if (update_ft_attr(cl,
                    attr, attr_len,
                    value, value_len) != 0)
            return -1;

        p+=value_len;
    }

#ifdef DEBUG
    syslog(LOG_DEBUG, "attributes received, sess #%ld", cl->session_id);
#endif

    return 0;
}

/** 
 * update the attribute with the given
 * value
 **/
static int
update_ft_attr(struct client *cl,
               char *attr, int attr_len,
               char *val, int val_len)
{
    int  e_len;
    int  len;
    char *q = malloc(128+(val_len*2)+attr_len);
    if (!q)
        return -1;
    /* we allocate val_len*2 because mysql_real_escape_string
     * could theoretically create a string double in size */

    len = sprintf(q, "UPDATE ft_%.64s SET %.*s = '",
            cl->filetype_name,
            attr_len,
            nol_s_str_filter_name(attr, attr_len));

    len += mysql_real_escape_string(cl->mysql, q+len, val, val_len);
    len += sprintf(q+len, "' WHERE id=%ld LIMIT 1;", cl->target_id);

    if (mysql_real_query(cl->mysql, q, len) != 0) {
        free(q);
        syslog(LOG_ERR, "updating attributes failed: %s",
                mysql_error(cl->mysql));
        return -1;
    }

    free(q);
    return 0;
}

/** 
 * COUNT reports the amount of matched URLs against
 * a filetype. Syntax:
 *
 * COUNT <filetype-name> <count>\n
 **/
static int
on_count(nolp_t *no, char *buf, int size)
{
    struct client *cl;
    char     *s;
    char      q[128];
    uint32_t  count;
    int       sz;

    cl = ((struct client *)no->private);
    if (!cl->running || !cl->session_id)
        return -1;
    if (!(s = memchr(buf, ' ', size)))
        return -1;

    *s = '\0';

    s++;
    count = (uint32_t)atoi(s);

    sz = sprintf(q, "UPDATE `nol_session` SET count_%s = %d WHERE id=%d",
                 nol_s_str_filter_name(buf, (s-1)-buf),
                 count,
                 cl->session_id);

    if (mysql_real_query(cl->mysql, q, sz) != 0) {
        syslog(LOG_ERR, "updating session statistics failed: %s",
                mysql_error(cl->mysql));
        return -1;
    }

    return 0;
}

/** 
 * filter a string by replacing all characters
 * but A-Za-z0-9_ with '_', required for 
 * filetype names, table names, login names
 * and more
 *
 * returns the given string
 **/
char *
nol_s_str_filter_name(char *s, int size)
{
    char *p = s;
    char *e = s+size;
    if (size == -1) {
        for (p=s;*p;p++)
            if (!isalnum(*p))
                *p = '_';
    } else {
        for (p=s;p<e;p++)
            if (!isalnum(*p))
                *p = '_';
    }
    return s;
}

/** 
 * filter a string by replacing all occurences
 * of '\'' with '_' to prevent SQL-injections
 *
 * returns the given string
 **/
char *
nol_s_str_filter_quote(char *s, int size)
{
    char *p = s;
    char *e = s+size;
    if (size == -1) {
        for (p=s;*p;p++)
            if (*p == '\'')
                *p = '_';
    } else {
        for (p=s;p<e;p++)
            if (*p == '\'')
                *p = '_';
    }
    return s;
}

