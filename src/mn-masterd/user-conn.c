/*-
 * user.c
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

#include "master.h"
#include "slave.h"
#include "nolp.h"

#include <ev.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int user_list_clients_command(nolp_t *no, char *buf, int size);
static int user_list_slaves_command(nolp_t *no, char *buf, int size);
static int user_slave_info_command(nolp_t *no, char *buf, int size);
static int user_client_info_command(nolp_t *no, char *buf, int size);
static int user_show_config_command(nolp_t *no, char *buf, int size);
static int user_log_command(nolp_t *no, char *buf, int size);
static int user_add_command(nolp_t *no, char *buf, int size);
static int user_useradd_command(nolp_t *no, char *buf, int size);
static int user_userdel_command(nolp_t *no, char *buf, int size);
static int user_passwd_command(nolp_t *no, char *buf, int size);
static int user_session_info_command(nolp_t *no, char *buf, int size);
static int user_session_report_command(nolp_t *no, char *buf, int size);
static int user_list_sessions_command(nolp_t *no, char *buf, int size);
static int user_list_input_command(nolp_t *no, char *buf, int size);
static int user_kill_all_command(nolp_t *no, char *buf, int size);
static int user_system_info_command(nolp_t *no, char *buf, int size);

struct nolp_fn user_commands[] = {
    {"LIST-SLAVES", &user_list_slaves_command},
    {"LIST-CLIENTS", user_list_clients_command},
    {"SLAVE-INFO", user_slave_info_command},
    {"CLIENT-INFO", user_client_info_command},
    {"SHOW-CONFIG", user_show_config_command},
    {"LOG", user_log_command},
    {"ADD", user_add_command},
    {"USERADD", user_useradd_command},
    {"USERDEL", user_userdel_command},
    {"PASSWD", user_passwd_command},
    {"SESSION-INFO", user_session_info_command},
    {"SESSION-REPORT", user_session_report_command},
    {"LIST-SESSIONS", user_list_sessions_command},
    {"LIST-INPUT", user_list_input_command},
    {"KILL-ALL", user_kill_all_command},
    {"SYSTEM-INFO", user_system_info_command},
    {0},
};

#define MSG100 "100 OK\n"
#define MSG200 "200 Denied\n"
#define MSG201 "201 Bad Request\n"
#define MSG202 "202 Login type unavailable\n"
#define MSG203 "203 Not found\n"
#define MSG300 "300 Internal Error\n"

/** 
 * The LIST-CLIENTS command requests a list of clients
 * connected to the given slave. Syntax:
 * LIST-CLIENTS <slave>\n
 * 'slave' should be the iid of a slave
 **/
static int
user_list_clients_command(nolp_t *no, char *buf, int size)
{
    int  x;
    int  id;
    char reply[32];
    struct conn *conn = (struct conn*)no->private;

    id = atoi(buf);

    for (x=0; ; x++) {
        if (x == srv.num_slaves) {
            send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
            break;
        }
        slave_conn_t *sl = &srv.slaves[x];
        if (sl->id == id) {
            int len = sprintf(reply, "100 %d\n", sl->xml.clients.sz);
            send(conn->sock, reply, len, 0);
            send(conn->sock, sl->xml.clients.buf, sl->xml.clients.sz, 0);
            break;
        }
    }

    return 0;
}

/** 
 * LIST-SLAVES returns a list of all slaves in XML format, 
 * including their client count. Syntax:
 * LIST-SLAVES 0\n
 * currently, the second argument is unused and should be 
 * set to 0.
 **/
static int
user_list_slaves_command(nolp_t *no, char *buf, int size)
{
    char reply[32];
    struct conn *conn = (struct conn*)no->private;
    int len = sprintf(reply, "100 %d\n", srv.xml.slave_list.sz);
    send(conn->sock, reply, len, 0);
    send(conn->sock, srv.xml.slave_list.buf, srv.xml.slave_list.sz, 0);

    return 0;
}

/** 
 * The SLAVE-INFO outputs information about a slave, such
 * as address and num connected clients, in XML format.
 * Syntax:
 * SLAVE-INFO <slave>\n
 * 'slave' should be the NAME of a slave
 **/
static int
user_slave_info_command(nolp_t *no, char *buf, int size)
{
    int x;
    int id;
    char tmp[32];
    char reply[
        sizeof("<slave-info for=\"\"><address></address></slave-info>")-1
        +64+20+16+6];
    struct conn  *conn = (struct conn*)no->private;
    slave_conn_t *sl = 0;

    id = atoi(buf);
    for (x=0; x<srv.num_slaves; x++) {
        if (srv.slaves[x].id == id) {
            sl = &srv.slaves[x];
            break;
        }
    }
    if (!sl) {
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }
    int len =
        sprintf(reply,
            "<slave-info for=\"%s-%d\">"
              "<address>%s:%hd</address>"
            "</slave-info>",
            sl->name,
            sl->id,
            (sl->ready ? sl->listen_addr : "0"),
            (sl->ready ? sl->listen_port : 0)
            );
    x = sprintf(tmp, "100 %d\n", len);
    send(conn->sock, tmp, x, 0);
    send(conn->sock, reply, len, 0);
    return 0;
}

/** 
 * The CLIENT-INFO outputs information about a client
 * in XML format. Syntax:
 * CLIENT-INFO <client>\n
 * 'client' should be the NAME of a client
 **/
static int
user_client_info_command(nolp_t *no, char *buf, int size)
{
    int x, y;
    int  found;
    int  sz;
    char out[256];
    slave_conn_t  *sl;
    struct conn   *conn;
    struct client *c;

    if (size != 40)
        return -1; /* return -1 to close the connection */

    found = 0;
    c = 0;
    conn = (struct conn*)no->private;

    /* search through all slaves for a client matching the 
     * given hash */
    for (x=0; x<srv.num_slaves; x++) {
        for (y=0; y<srv.slaves[x].num_clients; y++) {
            if (memcmp(srv.slaves[x].clients[y].token, buf, 40) == 0) {
                c = &srv.slaves[x].clients[y];
                sl = &srv.slaves[x];
                found = 1;
                break;
            }
        }
        if (found)
            break;
    }
    if (!c) {
        /* send 203 not found message if the client was not
         * found */
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    sz = sprintf(out, 
            "<client id=\"%.40s\">"
              "<user>%.64s</user>"
              "<slave>%.64s-%d</slave>"
              "<status>%u</status>"
              "<address>%.15s</address>"
              "<session>%u</session>"
            "</client>",
            c->token,
            c->user,
            sl->name, sl->id,
            (c->status & 1),
            c->addr, c->session_id);
    x = sprintf(out+sz, "100 %d\n", sz);

    send(conn->sock, out+sz, x, 0);
    send(conn->sock, out, sz, 0);
    return 0;
}

static int
user_show_config_command(nolp_t *no, char *buf, int size)
{
    char           out[64];
    struct conn   *conn;
    int            x;

    conn = (struct conn *)no->private;
    x = sprintf(out, "100 %d\n", srv.config_sz);
    send(conn->sock, out, x, 0);
    send(conn->sock, srv.config_buf, srv.config_sz, 0);
    return 0;
}

static int
user_log_command(nolp_t *no, char *buf, int size)
{
    return 0;
}

/** 
 * called when the ADD request is performed by
 * a user. ADD is used to add URLs.
 **/
static int
user_add_command(nolp_t *no, char *buf, int size)
{
    char q[size+32];
    char crawler[64];
    char *e;
    int  len;
    int  x;
    e = buf+size;
    buf[size] = '\0';
    if (!(len = sscanf(buf, "%64s", &crawler)))
        return -1;
    while (!isspace(*buf) && buf<e)
        buf++;
    while (isspace(*buf) && buf<e)
        buf++;
    /* make sure there's no single-quote that can 
     * risk injecting SQL */
    strrmsq(crawler);
    strrmsq(buf);
    len = sprintf(q, "INSERT INTO nol_added (user_id, crawler, input, date) "
                     "VALUES (%d, '%s', '%s', NOW())",
                     ((struct conn*)no->private)->user_id,
                     crawler,
                     buf);
    if (mysql_real_query(srv.mysql, q, len) != 0) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }

    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}

static int
user_useradd_command(nolp_t *no, char *buf, int size)
{
    
}

static int
user_userdel_command(nolp_t *no, char *buf, int size)
{

}

static int
user_passwd_command(nolp_t *no, char *buf, int size)
{

}

static int
user_session_info_command(nolp_t *no, char *buf, int size)
{
    uint32_t session_id;
    MYSQL_RES *r;
    MYSQL_ROW row;
    session_id = atoi(buf);
    char *out;
    int  len;

    len = asprintf(&out, 
            "SELECT "
                "`nol_session`.`date`, `latest`, `state`, "
                "`cl`.`token`, `a`.`crawler`, `a`.`input` "
            "FROM "
                "`nol_session` "
            "LEFT JOIN "
                "`nol_client` as `cl` "
              "ON "
                "`cl`.`id` = `nol_session`.`client_id` "
            "LEFT JOIN "
                "`nol_added` as `a` "
              "ON "
                "`a`.`id` = `nol_session`.`added_id` "
            "WHERE `nol_session`.`id` = %u LIMIT 0,1;",
            session_id);
    if (mysql_real_query(srv.mysql, out, len) != 0) {
        syslog(LOG_ERR, "selecting session info failed: %s",
                mysql_error(srv.mysql));
        return -1;
    }
    free(out);
    r = mysql_store_result(srv.mysql);
    if (r) {
        if ((row = mysql_fetch_row(r))) {
            len = asprintf(&out,
                    "<session-info for=\"%u\">"
                      "<started>%.19s</started>"
                      "<latest>%.19s</latest>"
                      "<state>%.20s</state>"
                      "<client>%40s</client>"
                      "<crawler>%.64s</crawler>"
                      "<input>%s</input>"
                    "</session-info>",
                    session_id, row[0], row[1],
                    row[2], row[3], row[4],
                    row[5]
                    );
            char tmp[64];
            int len2 = sprintf(tmp, "100 %u\n", len);
            send(no->fd, tmp, len2, 0);
            send(no->fd, out, len, 0);
            free(out);
        } else
            send(no->fd, MSG203, sizeof(MSG203)-1, 0);
        mysql_free_result(r);
    } else
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);

    return 0;
}

static int
user_session_report_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *r;
    MYSQL_ROW row;
    char b[56+12];
    int  sz;
    unsigned long *lengths;

    sz = sprintf(b,
            "SELECT `report` FROM `nol_session` "
            "WHERE `id`=%d LIMIT 0,1",
            atoi(buf));
    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "fetching session report failed: %s", 
                mysql_error(srv.mysql));
        return -1;
    }
    if (!(r = mysql_store_result(srv.mysql))) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    if (row = mysql_fetch_row(r)) {
        lengths = mysql_fetch_lengths(r);
        sz = sprintf(b, "100 %d\n", (int)lengths[0]);
        send(no->fd, b, sz, 0);
        send(no->fd, row[0], (int)lengths[0], 0);
    } else
        send(no->fd, MSG203, sizeof(MSG203)-1, 0);

    mysql_free_result(r);
    return 0;
}

/** 
 * SESSION-LIST <start> <count>\n
 **/
static int
user_list_sessions_command(nolp_t *no, char *buf, int size)
{
    int start, limit;
    char *b;
    MYSQL_RES *r;
    MYSQL_ROW row;
    int sz;

    if (sscanf(buf, "%d %d", &start, &limit) != 2) {
        send(no->fd, MSG201, sizeof(MSG201)-1, 0);
        return -1;
    }
    if (limit > 100) limit = 100;
    char **bufs = malloc(sizeof(char *)*limit);
    int   *sizes = malloc(sizeof(int)*limit);

    sz = asprintf(&b,
            "SELECT "
                "`S`.`id`, `S`.`latest`, `S`.`state`, "
                "`A`.crawler, `A`.input , `C`.`token`"
            "FROM `nol_session` as `S` "
            "LEFT JOIN "
                "`nol_added` as `A` "
              "ON "
                "`A`.`id` = `S`.`added_id` "
            "LEFT JOIN "
                "`nol_client` AS `C` "
              "ON "
                "`C`.`id` = `S`.`client_id` "
            "ORDER BY "
                "`S`.`latest` DESC "
            "LIMIT %d, %d;",
            start, limit);

    if (!b) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        syslog(LOG_ERR, "out of mem");
        return -1;
    }
    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "LIST-SESSIONS failed: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        free(b);
        return -1;
    }
    free(b);

    if ((r = mysql_store_result(srv.mysql))) {
        int x = 0;
        int total = 0;
        while (row = mysql_fetch_row(r)) {
            unsigned long *l;
            l = mysql_fetch_lengths(r);
            char *curr = malloc(l[0]+l[1]+l[2]+l[3]+l[4]+l[5]+
                    sizeof("<session id=\"\"><latest></latest><state></state><crawler></crawler><input></input><client></client></session>"));
            sz = sprintf(curr,
                    "<session id=\"%d\">"
                      "<latest>%s</latest>"
                      "<state>%s</state>"
                      "<crawler>%s</crawler>"
                      "<input>%s</input>"
                      "<client>%s</client>"
                    "</session>",
                    atoi(row[0]), row[1]?row[1]:"", row[2]?row[2]:"",
                    row[3]?row[3]:"", row[4]?row[4]:"", row[5]?row[5]:"");

            bufs[x] = curr;
            sizes[x] = sz;
            total += sz;
            x++;
        }
        limit = x;

        char *tmp;
        sz = asprintf(&tmp, "100 %d\n", total+sizeof("<session-list></session-list>")-1);
        send(no->fd, tmp, sz, 0);
        free(tmp);

        send(no->fd, "<session-list>", sizeof("<session-list>")-1, 0);
        for (x=0; x<limit; x++) {
            send(no->fd, bufs[x], sizes[x], 0);
            free(bufs[x]);
        }
        send(no->fd, "</session-list>", sizeof("</session-list>")-1, 0);
    } else 
        syslog(LOG_ERR, "mysql_store_result() failed");
    free(bufs);
    free(sizes);

    return 0;
}

/** 
 * LIST-INPUT\n
 *
 * list the input data by the current user
 **/
static int
user_list_input_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *r;
    MYSQL_ROW row;
    char b[128];
    struct conn *conn = (struct conn*)no->private;
    int   sz;
    long  count;

    sz = sprintf(b, "SELECT id, crawler, input FROM nol_added WHERE user_id=%d ORDER BY `id` DESC LIMIT 0,1000", conn->user_id);

    if (mysql_real_query(srv.mysql, b, sz) != 0) {
        syslog(LOG_ERR,
                "LIST-INPUT failed: %s",
                mysql_error(srv.mysql));
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }
    char **bufs;
    int   *sizes;
    if ((r = mysql_store_result(srv.mysql))) {
        count = (long)mysql_num_rows(r);
        bufs = malloc(sizeof(char *)*count);
        sizes = malloc(sizeof(int)*count);
        int x = 0;
        int total = 0;
        if (!bufs || !sizes)
            return -1;
        while (row = mysql_fetch_row(r)) {
            unsigned long *l;
            l = mysql_fetch_lengths(r);
            char *curr = malloc(l[0]+l[1]+l[2]+
                    sizeof("<input id=\"\"><crawler></crawler><value></value></input>"));
            sz = sprintf(curr,
                    "<input id=\"%d\">"
                      "<crawler>%s</crawler>"
                      "<value>%s</value>"
                    "</input>",
                    atoi(row[0]), row[1]?row[1]:"",
                    row[2]?row[2]:""
                    );
            bufs[x] = curr;
            sizes[x] = sz;
            total += sz;
            x++;
        }
        count = (long)x;

        char *tmp;
        sz = asprintf(&tmp, "100 %u\n", (unsigned int)total+sizeof("<input-list></input-list>")-1);
        send(no->fd, tmp, sz, 0);
        free(tmp);

        send(no->fd, "<input-list>", sizeof("<input-list>")-1, 0);
        for (x=0; x<count; x++) {
            send(no->fd, bufs[x], sizes[x], 0);
            free(bufs[x]);
        }
        send(no->fd, "</input-list>", sizeof("</input-list>")-1, 0);
    } else 
        syslog(LOG_ERR, "mysql_store_result() failed");
    free(bufs);
    free(sizes);

    return 0;
}

/** 
 * KILL-ALL <slave>\n
 *
 * Disconnects all clients connected to the given slave 
 * TODO: check permissions
 **/
static int
user_kill_all_command(nolp_t *no, char *buf, int size)
{
    int           id;
    int           x;
    slave_conn_t *sl = 0;

    id = atoi(buf);
    for (x=0; x<srv.num_slaves; x++) {
        if (srv.slaves[x].id == id) {
            sl = &srv.slaves[x];
            break;
        }
    }
    if (!sl) {
        send(sl->conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    send(sl->conn->sock, "KILL-ALL\n", 9, 0);
    return 0;
}

/** 
 * SYSTEM-INFO command is used to get an overview of
 * the running system
 * XML:
 *
 * <system-info>
 *   <uptime>... seconds since master started ...</uptime>
 *   <address>... listening address of master ...</address>
 *   <num-slaves>... num connected slaves ...</num-slaves>
 *   <num-sessions>... total session count ...</num-sessions>
 * </system-info>
 **/
static int
user_system_info_command(nolp_t *no, char *buf, int size)
{
    MYSQL_RES *res;
    MYSQL_ROW  row;
    time_t now;
    double uptime;
    int  sz, sz2;
    int  num_sessions = 0;
    char xml[sizeof("<system-info><uptime></uptime><address></address>"
                    "<num-slaves></num-slaves>"
                    "<num-sessions></num-sessions>"
                    "</system-info>")
                    +96];
    time(&now);
    uptime = difftime(now, srv.start_time);

    mysql_query(srv.mysql, "SELECT COUNT(*) FROM `nol_session`;");
    if (!(res = mysql_store_result(srv.mysql)))
        return -1;
    if (!(row = mysql_fetch_row(res))) {
        mysql_free_result(res);
        return -1;
    }
    num_sessions = atoi(row[0]);
    mysql_free_result(res);

    sz = sprintf(xml, "<system-info>"
                         "<uptime>%ld</uptime>"
                         "<address>%.15s:%hd</address>"
                         "<num-slaves>%d</num-slaves>"
                         "<num-sessions>%d</num-sessions>"
                      "</system-info>",
                      (long)uptime,
                      inet_ntoa(srv.addr.sin_addr),
                      htons(srv.addr.sin_port),
                      srv.num_slaves,
                      num_sessions);
    sz2 = sprintf(xml+sz, "100 %d\n", sz);
    send(no->fd, xml+sz, sz2, 0);
    send(no->fd, xml, sz, 0);
    return 0;
}
