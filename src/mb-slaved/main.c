/*-
 * main.c
 * This file is part of mb-slaved
 *
 * Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>
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
 * 
 * http://bithack.se/projects/methabot/
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ev.h>

#include "lmc.h"
#include "../mb-masterd/server.h"
#include "client.h"
#include "slave.h"
#include "nolp.h"
#include "hook.h"

int mbs_cleanup();
static void mbs_ev_sigint(EV_P_ ev_signal *w, int revents);
static int mbs_load_config();
static int mbs_master_login();
static int mbs_master_connect();
static void mbs_ev_master(EV_P_ ev_io *w, int revents);
static void mbs_ev_conn_accept(EV_P_ ev_io *w, int revents);
static void mbs_ev_master_timer(EV_P_ ev_io *w, int revents);
static void mbs_ev_client_status(EV_P_ ev_async *w, int revents);

const char *slave_init_cb(void);
const char *slave_run_cb(void);
void set_defaults(void);

const char     *_cfg_file;
struct slave    srv;
struct opt_vals opt_vals;
extern struct nolp_fn master_commands[];

struct lmc_scope slave_scope =
{
    "slave",
    {
        LMC_OPT_STRING("listen", &opt_vals.listen),
        LMC_OPT_STRING("master", &opt_vals.master),
        LMC_OPT_STRING("user", &opt_vals.user),
        LMC_OPT_STRING("group", &opt_vals.group),
        LMC_OPT_STRING("exec_dir", &opt_vals.exec_dir),
        LMC_OPT_STRING("mysql_host", &opt_vals.mysql_host),
        LMC_OPT_STRING("mysql_sock", &opt_vals.mysql_sock),
        LMC_OPT_STRING("mysql_user", &opt_vals.mysql_user),
        LMC_OPT_STRING("mysql_pass", &opt_vals.mysql_pass),
        LMC_OPT_STRING("mysql_db", &opt_vals.mysql_db),
        LMC_OPT_UINT("mysql_port", &opt_vals.mysql_port),
        LMC_OPT_END,
    }
};

int
main(int argc, char **argv)
{
    lmc_parser_t *lmc;
    int           r;
    int           nofork = 0;
    signal(SIGPIPE, SIG_IGN);

    _cfg_file = "/etc/mb-slaved.conf";
    if (argc > 1) {
        if (strcmp(argv[1], "--no-fork") == 0)
            nofork=1;
        else
            _cfg_file = argv[1];
    }

    if (!(lmc = lmc_create(&srv))) {
        fprintf(stderr, "out of mem\n");
        return 1;
    }

    lmc_add_scope(lmc, &slave_scope);
    if ((r = nol_server_launch(
                _cfg_file, lmc,
                &opt_vals.user, &opt_vals.group,
                &slave_init_cb, &slave_run_cb,
                nofork==1?0:1)) == 0)
        fprintf(stdout, "started\n");
    /* if nol_server_launch() does not return 0,
     * it should have printed an error message to stderr already */

    lmc_destroy(lmc);
    mbs_cleanup();
    return r;
}

/* called by nol_server_launch(), return 0 on success */
const char *
slave_init_cb(void)
{
    char *s;
    int   sock;
    int   o = 1;

    openlog("mb-slaved", LOG_PID, LOG_DAEMON);
    set_defaults();

    if (!(srv.master_io.data = srv.m_nolp = nolp_create(&master_commands, 0)))
        return "nolp init failed";
    if ((s = strchr(opt_vals.master, ':'))) {
        srv.master.sin_port = htons(atoi(s+1));
        *s = '\0';
    } else
        srv.master.sin_port = htons(5505);
    srv.master.sin_addr.s_addr = inet_addr(opt_vals.master);

    if ((s = strchr(opt_vals.listen, ':'))) {
        srv.addr.sin_port = htons(atoi(s+1));
        *s = '\0';
    } else
        srv.addr.sin_port = htons(NOL_SLAVE_DEFAULT_PORT);
    srv.addr.sin_addr.s_addr = inet_addr(opt_vals.listen);
    srv.addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o));
    if (bind(sock, (struct sockaddr*)&srv.addr, sizeof srv.addr) == -1) {
        syslog(LOG_ERR, "could not bind to %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return "bind() failed";
    }
    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return "listen() failed";
    }

    if (!(srv.mysql = mbs_dup_mysql_conn()))
        return "could not connect to mysql server";
    if (mbs_master_connect() != 0)
        return "could not connect to master server";
    if (mbs_master_login() != 0)
        return "loggin in to master failed";

    srv.listen_sock = sock;

    if (opt_vals.exec_dir)
        chdir(opt_vals.exec_dir);

    return 0;
}

/* set all options to their default values */
void
set_defaults(void)
{
    if (!opt_vals.listen)
        opt_vals.listen = "127.0.0.1";
    if (!opt_vals.master)
        opt_vals.master = "127.0.0.1";

    srv.user = strdup("default");
    srv.pass = strdup("default");
}

const char*
slave_run_cb(void)
{
    struct ev_loop *loop;
    ev_io           io_listen;
    ev_signal       sigint_listen;
    ev_signal       sigterm_listen;

    pthread_mutex_init(&srv.pending_lk, 0);
    pthread_mutex_init(&srv.clients_lk, 0);
    if (!(loop = ev_default_loop(EVFLAG_AUTO)))
        return 1;

    syslog(LOG_INFO, "listening on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));
    ev_signal_init(&sigint_listen, &mbs_ev_sigint, SIGINT);
    ev_signal_init(&sigterm_listen, &mbs_ev_sigint, SIGTERM);
    ev_io_init(&io_listen, &mbs_ev_conn_accept, srv.listen_sock, EV_READ);
    ev_io_init(&srv.master_io, &mbs_ev_master, srv.master_sock, EV_READ);
    ev_async_init(&srv.client_status, &mbs_ev_client_status);

    /* catch SIGINT so we can close connections and clean up properly */
    ev_signal_start(loop, &sigint_listen);
    ev_signal_start(loop, &sigterm_listen);
    ev_io_start(loop, &io_listen);
    ev_io_start(loop, &srv.master_io);
    ev_async_start(loop, &srv.client_status);

    /** 
     * This loop will listen for new connections and data from
     * the master.
     **/
    ev_loop(loop, 0);
    close(srv.listen_sock);

    ev_default_destroy();
    closelog();
    pthread_mutex_destroy(&srv.pending_lk);
    pthread_mutex_destroy(&srv.clients_lk);

    mbs_hook_invoke(HOOK_CLEANUP);
    return 0;
}

/** 
 * Create a new MySQL connection using the global
 * mysql settings, "duplicate" the connection
 **/
MYSQL *
mbs_dup_mysql_conn(void)
{
    MYSQL *ret;
    my_bool reconnect = 1;

    if (!(ret = mysql_init(0)))
        return 0;
    mysql_options(ret, MYSQL_OPT_RECONNECT, &reconnect);
    if (!(mysql_real_connect(ret,
                    opt_vals.mysql_host, opt_vals.mysql_user,
                    opt_vals.mysql_pass, opt_vals.mysql_db,
                    opt_vals.mysql_port, opt_vals.mysql_sock,
                    0))) {
        mysql_close(ret);
        return 0;
    }

    return ret;
}

/** 
 * A client connected, disconnected or changed status. 
 * Notify the master server about this change.
 *
 * Output buffer sent to the master will look like:
 * STATUS <x>\n
 * <token> <address> <user> <state> <sess-id>\n
 * <token> <address> <user> <state> <sess-id>\n
 * ...
 *
 * Where x is the size in bytes of the list of 
 * clients (starting from second line), token
 * is the 40-char client token, and state is 0/1 whether
 * the client is running or idle (1 = running).
 **/
static void
mbs_ev_client_status(EV_P_ ev_async *w,
                     int revents)
{
    char *out;
    char *p;
    int x;
    int len;
    pthread_mutex_lock(&srv.clients_lk);
    if (!(p = out = malloc(32+(srv.num_clients*(40+3+65+16)))))
        abort();
    for (x=0; x<srv.num_clients; x++)
        p += sprintf(p, "%.40s %s %s %u %u\n",
                srv.clients[x]->token,
                inet_ntoa(srv.clients[x]->addr),
                srv.clients[x]->user,
                (srv.clients[x]->running & 1),
                srv.clients[x]->session_id);
    pthread_mutex_unlock(&srv.clients_lk);
    len = p-out;
    x = sprintf(p, "STATUS %d\n", len);

    send(srv.master_io.fd, p, x, 0);
    send(srv.master_io.fd, out, len, 0);
    free(out);
}

/** 
 * Called when we have a new incoming connection
 *
 * For each connection we'll launch a new thread 
 * that will handle that specific connect. Only
 * clients will be able to connect to the slaves,
 * and the client must report a valid token.
 **/
static void
mbs_ev_conn_accept(EV_P_ ev_io *w, int revents)
{
    struct client *cl;
    struct sockaddr_in addr;
    int       sock;
    int       x;
    socklen_t sin_sz;

    addr.sin_family = AF_INET;

    if (!(cl = malloc(sizeof(struct client)))) {
        syslog(LOG_ERR, "out of mem");
        abort();
    }

    sin_sz = sizeof(struct sockaddr_in);
    if ((sock = accept(w->fd, &addr, &sin_sz)) == -1) {
        syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
    }

    if (!srv.num_pending) {
        send(sock, "200 Denied\n", 11, 0);
        close(sock);
    } else {
        for (x=0;; x++) {
            if (x == srv.num_pending) {
                send(sock, "200 Denied\n", 11, 0);
                close(sock);
                break;
            }
            if (addr.sin_addr.s_addr == srv.pending[x]->addr.s_addr) {
                pthread_t thr;
                if (pthread_create(&thr, 0, mbs_client_init, (void*)sock) != 0)
                    abort();
                break;
            }
        }
    }
}

/** 
 * Read one line from the given file descriptor
 **/
int
sock_getline(int fd, char *buf, int max)
{
    int sz;
    char *nl;

    if ((sz = recv(fd, buf, max, MSG_PEEK)) <= 0)
        return -1;
    if (!(nl = memchr(buf, '\n', sz)))
        return 0;
    if ((sz = read(fd, buf, nl-buf+1)) == -1)
        return -1;

    return sz;
}

/** 
 * Called when a message is sent from the server
 **/
static void
mbs_ev_master(EV_P_ ev_io *w, int revents)
{
    if ((revents & EV_ERROR) || nolp_recv(srv.m_nolp) != 0) {
        /* reach here if connection was closed or a socket error occured */
        syslog(LOG_WARNING, "master has gone away, reconnecting in 5s");
        close(w->fd);
        ev_io_stop(EV_A_ w);
        ev_timer_init(&srv.master_timer,
                &mbs_ev_master_timer,
                5.0f, 0.f);
        srv.master_timer.repeat = 5;
        ev_timer_start(EV_A_ &srv.master_timer);
    }
}

static void
mbs_ev_master_timer(EV_P_ ev_io *w, int revents)
{
    if (mbs_master_connect() == 0) {
        if (mbs_master_login() != 0)
            close(srv.master_sock);
        else {
            ev_timer_stop(EV_A_ &srv.master_timer);
            memset(&srv.master_io, 0, sizeof(ev_io));
            ev_io_init(&srv.master_io, &mbs_ev_master, srv.master_sock, EV_READ);
            ev_io_start(loop, &srv.master_io);
            return;
        }
    }

    ev_timer_again(EV_A_ &srv.master_timer);
}

static void
mbs_ev_sigint(EV_P_ ev_signal *w, int revents)
{
    ev_unloop(EV_A_ EVUNLOOP_ALL);
}

/** 
 * Connect to the master server specified in mb-slaved.conf
 *
 * 0 on success
 **/
static int
mbs_master_connect()
{
    if ((srv.master_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return 1;

    srv.master.sin_family = AF_INET;
    srv.m_nolp->fd = srv.master_sock;

    if (connect(srv.master_sock, &srv.master, sizeof(struct sockaddr_in)) != 0) {
        syslog(LOG_ERR, "could not connect to master: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

/** 
 * Login to the master
 *
 * 0 on success
 **/
static int
mbs_master_login()
{
    int len;
    int n;
    char str[128];
    char *p;
    if (strlen("AUTH slave ")+strlen(srv.user)
            +strlen(srv.pass)+3 >= 128) {

        syslog(LOG_ERR, "buffer exceeded");
        abort();
    }
    len = sprintf(str, "AUTH slave %s %s\n", srv.user, srv.pass);
    if (send(srv.master_sock, str, len, MSG_NOSIGNAL) == -1)
        return 1;
    if ((len = sock_getline(srv.master_sock, str, 127)) > 0
            && atoi(str) == 100) {
        syslog(LOG_INFO, "logged in to master");
        return 0;
    } else
        syslog(LOG_WARNING, "master returned code %d", atoi(str));

    return 1;
}

int
mbs_cleanup()
{
    if (srv.m_nolp) nolp_free(srv.m_nolp);
    if (opt_vals.listen) free(opt_vals.listen);
    if (opt_vals.master) free(opt_vals.master);
    if (opt_vals.mysql_host) free(opt_vals.mysql_host);
    if (opt_vals.mysql_sock) free(opt_vals.mysql_sock);
    if (opt_vals.mysql_user) free(opt_vals.mysql_user);
    if (opt_vals.mysql_pass) free(opt_vals.mysql_pass);
    if (opt_vals.mysql_db) free(opt_vals.mysql_db);
    if (opt_vals.user) free(opt_vals.user);
    if (opt_vals.group) free(opt_vals.group);
    if (opt_vals.exec_dir) free(opt_vals.exec_dir);
    if (srv.user) free(srv.user);
    if (srv.pass) free(srv.pass);
}

