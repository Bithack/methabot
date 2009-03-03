/*-
 * main.c
 * This file is part of mb-clientd 
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

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>

#include "client.h"
#include "../libmetha/libev/ev.c"

int sock_getline(int fd, char *buf, int max);
void mbc_ev_slave(EV_P_ ev_io *w, int revents);
void mbc_ev_master(EV_P_ ev_io *w, int revents);
static int mbc_master_login();
void mbc_ev_timer(EV_P_ ev_timer *w, int revents);

struct mbc mbc = {
    .state = MBC_STATE_DISCONNECTED,
};

const char *master = "127.0.0.1";
const char *user = "test";
const char *pass = "test";

int
main(int argc, char **argv)
{
    /** 
     * 1. Connect to master and log in
     * 2. Wait for slave token
     * 3. Connect to slave
     **/
    struct ev_loop *loop;
    signal(SIGPIPE, SIG_IGN);
    openlog("mb-clientd", 0, 0);

    if ((mbc.sock = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        syslog(LOG_ERR, "socket() failed: %s\n", strerror(errno));
        exit(1);
    }

    loop = ev_default_loop(0);
    ev_io_init(&mbc.sock_ev, mbc_ev_master, mbc.sock, EV_READ);
    ev_timer_init(&mbc.timer_ev, mbc_ev_timer, 10.f, .0f);

    if (mbc_master_connect() == 0) {
        mbc_master_login();
        mbc.state = MBC_STATE_WAIT_LOGIN;
        ev_io_start(loop, &mbc.sock_ev);
    } else {
        syslog(LOG_INFO, "master is away, retrying in 10 secs");
    }

    ev_loop(loop, 0);

    ev_default_destroy();
    return 0;
}

/** 
 * Callback from libmetha when a new url has been crawled
 **/
void
mbc_lm_status_cb(metha_t *m, worker_t *w, url_t *url)
{

}

/** 
 * Callback from libmetha when a target has been found
 **/
void
mbc_lm_target_cb(metha_t *m, worker_t *w,
                 url_t *url, filetype_t *ft)
{

}

void
mbc_ev_timer(EV_P_ ev_timer *w, int revents)
{
    abort();
    switch (mbc.state) {
    }

    ev_timer_again(EV_A_ &mbc.timer_ev);
}

void
mbc_ev_master(EV_P_ ev_io *w, int revents)
{
    char buf[256];
    int  sz;
    int  msg;

    if ((sz = sock_getline(w->fd, buf, 256)) <= 0) {
        /* disconnected from master before reply? */
        close(w->fd);
        ev_io_stop(EV_A_ w);
        syslog(LOG_ERR, "master has gone away");
    }

    switch (mbc.state) {
        case MBC_STATE_WAIT_LOGIN:
            /** 
             * Reply from master after login.
             **/
            if ((msg = atoi(buf)) == 101) {
                syslog(LOG_INFO, "logged on to master, waiting for token");
                mbc.state = MBC_STATE_WAIT_TOKEN;
            } else {
                /* failed to log in, TODO: try again in 10 seconds */
                abort();
            }
            break;

        case MBC_STATE_WAIT_TOKEN:
            if (sz>6 && memcmp(buf, "TOKEN", 5) == 0) {
                close(mbc.sock);
                ev_io_stop(EV_A_ w);
                syslog(LOG_INFO, "got token, logging on to slave");
                if (mbc_slave_connect(buf+6, sz-6) != 0) {
                    /* failed to log in, TODO: try again in 10 seconds */
                    abort();
                }
                ev_io_init(w, mbc_ev_slave, w->fd, EV_READ);
                ev_io_start(EV_A_ w);

            } else {
                abort();
            }
            break;
    }
}

void
mbc_ev_slave(EV_P_ ev_io *w, int revents)
{

}

/** 
 * Decode the given token, connect to the slave
 * and send the token as login.
 *
 * Format of the token:
 * xxxx-xxxx-xxxx-xxxx-a.b.c.d:Z
 * \_________________/ \_____/ |
 *         |             /     |
 *    Login token    IP-Addr Port
 * 
 * mbc.sock must be closed before this function is 
 * called, i.e. the master must have been disconnected.
 * The socket is reused.
 **/
int
mbc_slave_connect(const char *token, int len)
{
    const char *p, *s;
    const char *e;
    int         x = 0;
    char        out[25];

    e = (p = token)+len;
    do {
        if (!(p = memchr(p, '-', e-p)))
            return -1;
        x++;
        p++;
    } while (x<4);

    if (!(s = memchr(p, ':', e-p)))
        return -1;

    s++;
    /* token is now at 'token', address is at 'p', and
     * port is at 's' */
    mbc.slave.sin_port = htons(atoi(s));
    mbc.slave.sin_addr.s_addr = inet_addr(p);
    mbc.sock = socket(AF_INET, SOCK_STREAM, 0);

    if (connect(mbc.sock, (struct sockaddr*)&mbc.slave, sizeof(struct sockaddr_in)) != 0) {
#ifdef DEBUG
        syslog(LOG_ERR, "connect() failed: %s", strerror(errno));
#endif
        return -1;
    }

    sprintf(out, "AUTH %19s\n", token);
    send(mbc.sock, out, 25, 0);

    return 0;
}

/** 
 * Connect to the master, return 0 on success
 **/
int
mbc_master_connect()
{
    mbc.master.sin_family      = AF_INET;
    mbc.master.sin_port        = htons(5304);
    mbc.master.sin_addr.s_addr = inet_addr(master);

    return connect(mbc.sock, (struct sockaddr*)&mbc.master, sizeof(struct sockaddr_in));
}

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
 * Login to the master
 **/
static int
mbc_master_login()
{
    send(mbc.sock, "AUTH client test test\n", strlen("AUTH client test test\n"), 0);
}

