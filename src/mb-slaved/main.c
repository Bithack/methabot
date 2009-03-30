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

#include "client.h"
#include "slave.h"

int mbs_main();
int mbs_cleanup();
static void mbs_ev_sigint(EV_P_ ev_signal *w, int revents);
static int mbs_load_config();
static int mbs_master_login();
static int mbs_master_connect();
static void mbs_ev_master(EV_P_ ev_io *w, int revents);
static void mbs_ev_conn_accept(EV_P_ ev_io *w, int revents);
static void mbs_ev_master_timer(EV_P_ ev_io *w, int revents);
static void mbs_ev_client_status(EV_P_ ev_async *w, int revents);

struct slave srv = {
};

#include "../libmetha/libev/ev.c"

int
main(int argc, char **argv)
{
    int r=0;

    openlog("mb-slaved", 0, 0);
    syslog(LOG_INFO, "started");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    signal(SIGPIPE, SIG_IGN);

    if (!(srv.mysql = mysql_init(0)))
        abort();
    if (!(mysql_real_connect(srv.mysql, "localhost", "methanol", "test", "methanol", 0, "/var/run/mysqld/mysqld.sock", 0)))
        abort();

    do {
        pthread_mutex_init(&srv.pending_lk, 0);
        pthread_mutex_init(&srv.clients_lk, 0);

        if ((r = mbs_load_config()) != 0)
            break;
        if ((r = mbs_master_connect()) != 0)
            break;
        if ((r = mbs_master_login()) != 0)
            break;
        if ((r = mbs_main()) != 0)
            break;
    } while (0);

    mbs_cleanup();
    closelog();
    return r;
}

int mbs_main()
{
    struct ev_loop *loop;
    int sock;

    ev_io     io_listen;
    ev_signal sigint_listen;
    
    if (!(loop = ev_default_loop(EVFLAG_AUTO)))
        return 1;

    srv.addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (bind(sock, (struct sockaddr*)&srv.addr, sizeof srv.addr) == -1) {
        syslog(LOG_ERR, "could not bind to %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    syslog(LOG_INFO, "listening on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));

    ev_signal_init(&sigint_listen, &mbs_ev_sigint, SIGINT);
    ev_io_init(&io_listen, &mbs_ev_conn_accept, sock, EV_READ);
    ev_io_init(&srv.master_io, &mbs_ev_master, srv.master_sock, EV_READ);
    ev_async_init(&srv.client_status, &mbs_ev_client_status);

    /* catch SIGINT so we can close connections and clean up properly */
    ev_signal_start(loop, &sigint_listen);
    ev_io_start(loop, &io_listen);
    ev_io_start(loop, &srv.master_io);
    ev_async_start(loop, &srv.client_status);

    /** 
     * This loop will listen for new connections and data from
     * the master.
     **/
    ev_loop(loop, 0);
    close(sock);

    ev_default_destroy();

    return 0;
}

/** 
 * A client connected, disconnected or changed status. 
 * Notify the master server about this change.
 *
 * Output buffer sent to the master will look like:
 * STATUS <x>\n
 * <token> <address> <user> <state>\n
 * <token> <address> <user> <state>\n
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
        p += sprintf(p, "%.40s %s %s %d\n",
                srv.clients[x]->token,
                inet_ntoa(srv.clients[x]->addr),
                srv.clients[x]->user,
                (srv.clients[x]->running & 1));
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

    char buf[256];
    char *e, *p;
    int sz;

    if (revents & EV_ERROR) {
        close(w->fd);
        goto closed;
    }

    switch (srv.mstate) {
        case SLAVE_MSTATE_RECV_CONF:
            if ((sz = recv(w->fd, srv.config_buf, srv.config_cap-srv.config_sz, 0)) == -1)
                goto closed;
            if (!sz)
                goto closed;
            srv.config_sz+=sz;
            if (srv.config_sz == srv.config_cap) {
                syslog(LOG_INFO, "read config from master");
                srv.mstate = SLAVE_MSTATE_COMMAND;
            }
            break;

        case SLAVE_MSTATE_COMMAND:
            if ((sz = sock_getline(w->fd, buf, 255)) <= 0)
                goto closed;

            buf[sz-1] = '\0';

            if (strncmp(buf, "CONFIG", 6) == 0) {
                if (!(srv.config_buf = malloc(atoi(buf+6)))) {
                    syslog(LOG_ERR, "out of mem");
                    abort();
                }

                srv.config_cap = atoi(buf+6);
                srv.mstate = SLAVE_MSTATE_RECV_CONF;
            } else if (strncmp(buf, "CLIENT", 6) == 0) {
                char out[TOKEN_SIZE+5];
                char *s = strchr(buf+7, ' ');
                struct client *cl;
                int ok = 0;
                
                if (!s)
                    goto data_error;
                cl = mbs_client_create(buf+7, s+1);

                pthread_mutex_lock(&srv.pending_lk);
                if (cl && srv.num_pending < MAX_NUM_PENDING) {
                    ok = 1;

                    /* add this client to the pending list */
                    if (!(srv.pending = realloc(srv.pending,
                                    (srv.num_pending+1)*sizeof(struct client*)))) {
                        syslog(LOG_ERR, "out of mem");
                        abort();
                    }

                    srv.pending[srv.num_pending] = cl;
                    srv.num_pending ++;
                }
                pthread_mutex_unlock(&srv.pending_lk);

                if (!ok) {
                    send(w->fd, "400\n", 3, 0);
                    mbs_client_free(cl);
                } else {
                    sz = sprintf(out, "100 %40s\n", cl->token);
                    send(w->fd, out, sz, 0);
                }
            } else
                goto data_error;
            break;
    }

    return;

closed:
/*
        ev_io_stop(EV_A_ w);
        ev_timer_init(&srv.master_timer, &mbs_ev_master_timer, 20.0f, 0.f);
        ev_timer_start(EV_A_ &srv.master_timer);*/
    syslog(LOG_WARNING, "master has gone away");
    ev_io_stop(EV_A_ w);
    return;

data_error:
    syslog(LOG_ERR, "weird data from master");
    abort();
}

static void
mbs_ev_master_timer(EV_P_ ev_io *w, int revents)
{

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
    pthread_mutex_destroy(&srv.pending_lk);
    pthread_mutex_destroy(&srv.clients_lk);
    if (srv.user) free(srv.user);
    if (srv.pass) free(srv.pass);
}

static int
mbs_load_config()
{
    FILE *fp;
    char in[256];
    char *p;
    char *s;
    int len;

    if (!(fp = fopen("/etc/mb-slaved.conf", "r"))) {
        syslog(LOG_ERR, "could not open configuration file");
        return 1;
    }

    while (fgets(in, 256, fp)) {
        if (in[0] == '#')
            continue;
        p = in;
        in[strlen(in)-1] = '\0';
        while (isspace(*p))
            p++;
        s = p;
        if (p = strchr(p, '=')) {
            len = p-in;
            while (isspace(in[len-1]))
                len--;
            do p++; while (isspace(*p));

            int unknown = 1;
            switch (len) {
                case 5:
                    if (strncmp(in, "login", 5) == 0) {
                        if (!(s = strchr(p, ':'))) {
                            syslog(LOG_ERR, "login syntax incorrect (user:pwd)");
                            goto error;
                        }
                        *s = '\0';
                        srv.user = strdup(p);
                        srv.pass = strdup(s+1);
                        unknown = 0;
                    }
                    break;
                case 6:
                    if (strncmp(in, "master", 6) == 0) {
                        if ((s = strchr(p, ':'))) {
                            srv.master.sin_port = htons(atoi(s+1));
                            *s = '\0';
                        } else
                            srv.master.sin_port = htons(5304);
                        srv.master.sin_addr.s_addr = inet_addr(p);

                        unknown = 0;
                    } else if (strncmp(in, "listen", 6) == 0) {
                        if ((s = strchr(p, ':'))) {
                            srv.addr.sin_port = htons(atoi(s+1));
                            *s = '\0';
                        } else
                            srv.addr.sin_port = htons(MBS_DEFAULT_PORT);
                        srv.addr.sin_addr.s_addr = inet_addr(p);

                        unknown = 0;
                    }
                    break;
            }

            if (unknown) {
                for (s=in; isalnum(*s) || *s == '_'; s++)
                    ;
                *s = '\0';
                syslog(LOG_ERR, "unknown option '%s'", in);
                goto error;
            }
        } else if (strlen(s)) {
            syslog(LOG_ERR, "missing argument for '%s'", in);
            goto error;
        }
    }

    fclose(fp);
    return 0;

error:
    fclose(fp);
    return 1;
}

