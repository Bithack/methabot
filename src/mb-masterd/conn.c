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
#include "nolp.h"

#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

extern struct nolp_fn slave_commands[];

const char *auth_types[] = {
    "client",
    "slave",
    "user",
};

static void slave_read(EV_P_ ev_io *w, int revents);
static int mbm_token_reply(nolp_t *no, char *buf, int size);
/* user.c */
void mbm_user_read(EV_P_ ev_io *w, int revents);

static void conn_read(EV_P_ ev_io *w, int revents);
static int upgrade_conn(struct conn *conn);

/** 
 * Accept a connection and set up a conn struct
 **/
void
mbm_ev_conn_accept(EV_P_ ev_io *w, int revents)
{
    int sock;
    socklen_t sin_sz;

    struct conn *c = malloc(sizeof(struct conn));

    memset(&c->addr, 0, sizeof(struct sockaddr_in));
    sin_sz = sizeof(struct sockaddr_in);

    if (!c || !(srv.pool = realloc(srv.pool, sizeof(struct conn)*(srv.num_conns+1)))) {
        syslog(LOG_ERR, "fatal: out of mem");
        abort();
    }

    srv.pool[srv.num_conns] = c;
    srv.num_conns ++;

    if ((sock = accept(w->fd, (struct sockaddr *)&c->addr, &sin_sz)) == -1) {
        syslog(LOG_ERR, "fatal: accept() failed: %s", strerror(errno));
        abort();
    }

    syslog(LOG_INFO, "accepted connection from %s", inet_ntoa(c->addr.sin_addr));

    c->sock = sock;
    c->auth = 0;
    c->authenticated = 0;

    /* add an event listenr for this sock */
    ev_io_init(&c->fd_ev, &conn_read, sock, EV_READ);
    ev_io_start(EV_A_ &c->fd_ev);

    c->fd_ev.data = c;
}

int
mbm_getline(int fd, char *buf, int max)
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
 * Called when data is available on a socket 
 * connected to a slave.
 **/
static void
slave_read(EV_P_ ev_io *w, int revents)
{
    nolp_t *no = (nolp_t *)w->data;

    if (nolp_recv(no) != 0) {
        ev_io_stop(EV_A_ w);
        mbm_conn_close(no->private);
        /* TODO: free slave */
    }
}

static void
conn_read(EV_P_ ev_io *w, int revents)
{
    int sock = w->fd;
    char buf[256];
    int sz;
    char user[16];
    char pwd[16];
    char type[16];
    int x;

    struct conn* conn = w->data;
    struct slave *sl;

    if ((sz = mbm_getline(sock, buf, 255)) <= 0)
        goto close;

    buf[sz] = '\0';
    char *p = buf;

    if (conn->authenticated) {
        /* user & slave connections will change event callback
         * to the ones found in slave.c and user.c */
        goto close;
    } else {
        if (memcmp(buf, "AUTH", 4) != 0)
            goto denied;
        sscanf(p+4, "%15s %15s %15s", type, user, pwd);
        /* verify the auth type */
        int auth_found = 0;
        for (x=0; x<NUM_AUTH_TYPES; x++) {
            if (strcmp(type, auth_types[x]) == 0) {
                conn->auth = x;
                auth_found = 1;
                break;
            }
        }
        
        if (!auth_found)
            goto denied;

        /* first argument is the type, this can be client, slave, status or operator */
        syslog(LOG_INFO, "AUTH type=%s,user=%s OK from #%d", type, user, sock);
        conn->authenticated = 1;

        send(sock, "100 OK\n", 7, 0);

        if (upgrade_conn(conn) != 0)
            goto close;
    }
    return;


denied:
    send(sock, "200 DENIED\n", 11, MSG_NOSIGNAL);
close:
    ev_io_stop(EV_A_ &conn->fd_ev);
    mbm_conn_close(conn);
    return;

invalid:
    syslog(LOG_WARNING, "invalid data from #%d, closing connection", sock);
    send(sock, "201 Bad Request\n", 16, MSG_NOSIGNAL);
    close(sock);
}

/** 
 * upgrade the connection depending on what value
 * conn->auth is set to. Change the event loop
 * callback function and create a slave structure
 * if the conn is a slave
 *
 * return 0 unless an error occurs
 **/
static int
upgrade_conn(struct conn *conn)
{
    int  sock = conn->sock;
    int  sz;
    char buf[32];

    switch (conn->auth) {
        case MBM_AUTH_TYPE_CLIENT:

            /* give this client to a slave */
            if (!srv.num_slaves) {
                /* add to pending list */
                return 1;
            } else {
                sz = sprintf(buf, "CLIENT %s\n", inet_ntoa(conn->addr.sin_addr));
                send(srv.slaves[0].conn->sock, buf, sz, 0);
                srv.slaves[0].client_conn = conn;
                nolp_expect_line((nolp_t *)(srv.slaves[0].conn->fd_ev.data),
                        &mbm_token_reply);
            }
            break;

        case MBM_AUTH_TYPE_SLAVE:
            sprintf(buf, "CONFIG %d\n", srv.config_sz);
            send(sock, buf, strlen(buf), MSG_NOSIGNAL);
            send(sock, srv.config_buf, srv.config_sz, MSG_NOSIGNAL);

            /* Add this slave to the list of slaves */
            if (!(srv.slaves = realloc(srv.slaves, (srv.num_slaves+1)*sizeof(struct slave)))) {
                syslog(LOG_ERR, "out of mem");
                abort();
            }

            srv.slaves[srv.num_slaves].num_clients = 0;
            srv.slaves[srv.num_slaves].clients = 0;
            srv.slaves[srv.num_slaves].conn = conn;
            conn->slave_n = srv.num_slaves;
            srv.num_slaves ++;
            ev_set_cb(&conn->fd_ev, &slave_read);

            nolp_t *no = nolp_create(slave_commands, sock);
            no->private = conn;
            conn->fd_ev.data = no;
            break;

        case MBM_AUTH_TYPE_USER:
            ev_set_cb(&conn->fd_ev, &mbm_user_read);
            break;

        default:
            return 1;
    }

    return 0;
}

void
mbm_conn_close(struct conn *conn)
{
    int x;

    close(conn->sock);
    free(conn);

    for (x=0; x<srv.num_conns; x++) {
        if (srv.pool[x] == conn) {
            srv.pool[x] = srv.pool[srv.num_conns-1];
            break;
        }
    }

    srv.num_conns --;
}


/** 
 * return 0 if the token was read and sent to the 
 * corresponding client successfully
 **/
static int
mbm_token_reply(nolp_t *no, char *buf, int size)
{
    int           sz;
    char          out[70];
    struct conn  *client;
    struct conn  *conn = (struct conn*)no->private;
    struct slave *sl = &srv.slaves[conn->slave_n];
    client = sl->client_conn;

    if (atoi(buf) != 100)
        return -1;
    sz = sprintf(out, "TOKEN %.40s-%s:%hd\n", buf+4,
            inet_ntoa(conn->addr.sin_addr),
            5305);
    send(client->sock, out, sz, 0);

    /* stop and disconnect the client connection */
    ev_io_stop(EV_DEFAULT, &client->fd_ev);
    mbm_conn_close(client);

    return 0;
}

