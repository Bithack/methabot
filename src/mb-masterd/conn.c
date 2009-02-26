/*-
 * conn.c
 * This file is part of mb-masterd
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

#include "conn.h"
#include "master.h"

#include <syslog.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

const char *auth_types[] = {
    "client",
    "slave",
    "status",
    "operator",
};

static void mbm_ev_conn_read(EV_P_ ev_io *w, int revents);
static void mbm_conn_close(struct conn *conn);

/** 
 * Accept a connection and set up a conn struct
 **/
void
mbm_ev_conn_accept(EV_P_ ev_io *w, int revents)
{
    int sock;
    struct sockaddr_in addr;
    socklen_t sin_sz;

    memset(&addr, 0, sizeof(struct sockaddr_in));
    sin_sz = sizeof(struct sockaddr_in);

    if ((sock = accept(w->fd, (struct sockaddr *)&addr, &sin_sz)) == -1) {
        syslog(LOG_ERR, "fatal: accept() failed: %s", strerror(errno));
        abort();
    }

    syslog(LOG_INFO, "accepted connection from %s", inet_ntoa(addr.sin_addr));

    struct conn *c = malloc(sizeof(struct conn));

    if (!c || !(srv.pool = realloc(srv.pool, sizeof(struct conn)*(srv.num_conns+1)))) {
        syslog(LOG_ERR, "fatal: out of mem");
        abort();
    }

    srv.pool[srv.num_conns] = c;
    srv.num_conns ++;

    c->sock = sock;
    c->auth = 0;
    c->authenticated = 0;

    /* add an event listenr for this sock */
    ev_io_init(&c->fd_ev, &mbm_ev_conn_read, sock, EV_READ);
    ev_io_start(EV_A_ &c->fd_ev);

    c->fd_ev.data = c;
}

static void
mbm_ev_conn_read(EV_P_ ev_io *w, int revents)
{
    int sock = w->fd;
    char buf[256];
    int sz;
    char user[16];
    char pwd[16];
    char type[16];
    int auth;
    int x;

    struct conn* conn = w->data;

    if ((sz = recv(sock, buf, 255, MSG_NOSIGNAL)) == -1)
        return;

    buf[sz] = '\0';
    char *p = buf;

    if (conn->authenticated) {
        switch (auth) {
            case MBM_AUTH_TYPE_SLAVE:
            case MBM_AUTH_TYPE_STATUS:
            case MBM_AUTH_TYPE_OPERATOR:
                break;
            default:
                send(sock, "300 Internal Error\n", 19, MSG_NOSIGNAL);
                goto close;
        }
    } else {
        if (memcmp(buf, "AUTH", 4) == 0) {
            sscanf(p+4, "%15s %15s %15s", type, user, pwd);

            /* verify the auth type */
            int auth_found = 0;
            for (x=0; x<NUM_AUTH_TYPES; x++) {
                if (strcmp(type, auth_types[x]) == 0) {
                    auth = x;
                    auth_found = 1;
                    break;
                }
            }
            
            if (auth_found) {
                /* first argument is the type, this can be client, slave, status or operator */
                syslog(LOG_INFO, "AUTH type=%s,user=%s OK from #%d", type, user, sock);
                conn->authenticated = 1;

                switch (auth) {
                    case MBM_AUTH_TYPE_CLIENT:
                        send(sock, "101 OK\n", 7, MSG_NOSIGNAL);
                        send(sock, "SLAVE a792bf5f-a23b-1,127.0.0.1:5001\n", strlen("SLAVE a792bf5f-a23b-1,127.0.0.1:5001\n"), MSG_NOSIGNAL);
                        break;

                    case MBM_AUTH_TYPE_SLAVE:
                        send(sock, "100 OK\n", 7, MSG_NOSIGNAL);
                        send(sock, "CLIENT a792bf5f-a23b-1,127.0.0.1:5001\n", strlen("CLIENT a792bf5f-a23b-1,127.0.0.1:5001\n"), MSG_NOSIGNAL);
                        break;

                    default:
                        send(sock, "202 Login type not supported\n", strlen("202 Login type not supported\n"), MSG_NOSIGNAL);
                        goto close;
                }
            } else {
                send(sock, "200 DENIED\n", 11, MSG_NOSIGNAL);
                goto close;
            }
        } else
            goto invalid;
    }
    return;

close:
    ev_io_stop(EV_A_ &conn->fd_ev);
    mbm_conn_close(conn);
    return;

invalid:
    syslog(LOG_WARNING, "invalid data from #%d, closing connection", sock);
    send(sock, "201 Bad Request\n", 16, MSG_NOSIGNAL);
    close(sock);
}

static void
mbm_conn_close(struct conn *conn)
{
    int x;

    close(conn->sock);
    free(conn);

    for (x=0; x<srv.num_conns; x++) {
        if (srv.pool[x] == conn) {
            srv.pool[x] = srv.pool[srv.num_conns];
            break;
        }
    }

    srv.num_conns --;
}

