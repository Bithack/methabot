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
#include "../libmetha/metha.h"
#include "../libmetha/worker.h"
#include "../libmetha/libev/ev.c"

#define TIMER_WAIT 5.f

/* see mbc_set_active() */
enum {
    MBC_NONE,
    MBC_MASTER,
    MBC_SLAVE,
};

int sock_getline(int fd, char *buf, int max);
void mbc_ev_slave(EV_P_ ev_io *w, int revents);
void mbc_ev_master(EV_P_ ev_io *w, int revents);
static int mbc_master_send_login();
void mbc_ev_timer(EV_P_ ev_timer *w, int revents);
void mbc_set_active(EV_P_ int which);

static void mbc_lm_status_cb(metha_t *m, worker_t *w, url_t *url);
static void mbc_lm_target_cb(metha_t *m, worker_t *w, url_t *url, filetype_t *ft);
static void mbc_lm_error_cb(metha_t *m, const char *s, ...);
static void mbc_lm_warning_cb(metha_t *m, const char *s, ...);
static void mbc_lm_ev_cb(metha_t *m, int ev);

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

    if (!(mbc.m = lmetha_create()))
        exit(1);

    lmetha_setopt(mbc.m, LMOPT_TARGET_FUNCTION, mbc_lm_target_cb);
    lmetha_setopt(mbc.m, LMOPT_STATUS_FUNCTION, mbc_lm_status_cb);
    lmetha_setopt(mbc.m, LMOPT_ERROR_FUNCTION, mbc_lm_error_cb);
    lmetha_setopt(mbc.m, LMOPT_WARNING_FUNCTION, mbc_lm_warning_cb);
    lmetha_setopt(mbc.m, LMOPT_EV_FUNCTION, mbc_lm_ev_cb);

    lmetha_start(mbc.m);

    loop = ev_default_loop(0);

    ev_io_init(&mbc.sock_ev, mbc_ev_master, mbc.sock, EV_READ);
    ev_timer_init(&mbc.timer_ev, mbc_ev_timer, TIMER_WAIT, .0f);
    mbc.timer_ev.repeat = TIMER_WAIT;

    if (mbc_master_connect() == 0) {
        mbc_master_send_login();
        mbc.state = MBC_STATE_WAIT_LOGIN;
        mbc_set_active(loop, MBC_MASTER);
    } else {
        ev_timer_start(loop, &mbc.timer_ev);
        syslog(LOG_INFO, "master is away, retrying in 10 secs");
    }

    ev_loop(loop, 0);

    ev_default_destroy();
    return 0;
}

/** 
 * Callback from libmetha when a new url has been crawled
 **/
static void
mbc_lm_status_cb(metha_t *m, worker_t *w, url_t *url)
{

}

/** 
 * Callback from libmetha when a target has been found
 **/
static void
mbc_lm_target_cb(metha_t *m, worker_t *w,
                 url_t *url, filetype_t *ft)
{

}

static void
mbc_lm_error_cb(metha_t *m, const char *s, ...)
{

}

static void
mbc_lm_warning_cb(metha_t *m, const char *s, ...)
{

}

static void
mbc_lm_ev_cb(metha_t *m, int ev)
{

}

/** 
 * Set to MBC_NONE/MBC_MASTER/MBC_SLAVE depending on
 * which one we are going to communicate with.
 **/
void
mbc_set_active(EV_P_ int which)
{
    if (ev_is_active(&mbc.sock_ev))
        ev_io_stop(EV_A_ &mbc.sock_ev);
    if (ev_is_active(&mbc.timer_ev))
        ev_timer_stop(EV_A_ &mbc.timer_ev);

    switch (which) {
        case MBC_NONE:
            /* Start a timer so we don't freeze, this timer
             * will reconnect to the master after 10 secs */
            ev_timer_start(EV_A_ &mbc.timer_ev);
            break;

        case MBC_MASTER:
            ev_io_init(&mbc.sock_ev, mbc_ev_master, mbc.sock, EV_READ);
            ev_io_start(EV_A_ &mbc.sock_ev);
            break;

        case MBC_SLAVE:
            ev_io_init(&mbc.sock_ev, mbc_ev_slave, mbc.sock, EV_READ);
            ev_io_start(EV_A_ &mbc.sock_ev);
            break;
    }
}

void
mbc_ev_timer(EV_P_ ev_timer *w, int revents)
{
    switch (mbc.state) {
        case MBC_STATE_DISCONNECTED:
            syslog(LOG_INFO, "reconnecting to master");
            if (mbc_master_connect() == 0) {
                mbc_master_send_login();
                mbc.state = MBC_STATE_WAIT_LOGIN;
                mbc_set_active(EV_A_ MBC_MASTER);

                break;
            } else
                syslog(LOG_WARNING, "could not connect to master");

        default:
            ev_timer_again(EV_A_ &mbc.timer_ev);
            break;
    }
}

void
mbc_ev_master(EV_P_ ev_io *w, int revents)
{
    char buf[256];
    int  sz;
    int  msg;
    int fail = 0;

    if ((sz = sock_getline(w->fd, buf, 256)) <= 0) {
        /* disconnected from master before reply? */
        close(w->fd);
        syslog(LOG_ERR, "master has gone away");
        fail = 1;
    } else {
        switch (mbc.state) {
            case MBC_STATE_WAIT_LOGIN:
                /** 
                 * Reply from master after login.
                 **/
                if ((msg = atoi(buf)) == 101) {
                    syslog(LOG_INFO, "logged on to master, waiting for token");
                    mbc.state = MBC_STATE_WAIT_TOKEN;
                } else
                    fail = 1;
                break;

            case MBC_STATE_WAIT_TOKEN:
                if (sz>6 && memcmp(buf, "TOKEN", 5) == 0) {
                    close(mbc.sock);
                    syslog(LOG_INFO, "got token, logging on to slave");

                    if (mbc_slave_connect(buf+6, sz-6) == 0) {
                        mbc_set_active(EV_A_ MBC_SLAVE);
                        break;
                    }
                }

                /* reach here if anything failed */
                syslog(LOG_WARNING, "could not connect to slave");
                fail = 1;
                break;
        }
    }

    if (fail) {
        close(mbc.sock);
        mbc_set_active(EV_A_ MBC_NONE);
    }
}

/** 
 * Called when a message from the slave is sent
 **/
void
mbc_ev_slave(EV_P_ ev_io *w, int revents)
{
    char buf[256];
    int  sz;
    int  msg;
    int disconnect = 0;

    if ((sz = sock_getline(w->fd, buf, 255)) <= 0) {
        syslog(LOG_WARNING, "slave has gone away");
        disconnect = 1;
    } else {
        switch (mbc.state) {
            case MBC_STATE_WAIT_LOGIN:
                if ((msg = atoi(buf)) == 100) {
                    syslog(LOG_INFO, "slave connection established");
                    mbc.state = MBC_STATE_STOPPED;
                } else {
                    syslog(LOG_WARNING, "slave login failed");
                    disconnect = 1;
                }
                break;

            case MBC_STATE_RUNNING:
                break;
        }
    }

    if (disconnect) {
        close(w->fd);
        mbc.state = MBC_STATE_DISCONNECTED;
        mbc_set_active(EV_A_ MBC_NONE);
    }
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

    char *t;

    e = (p = token)+len;
    do {
        if (!(p = memchr(p, '-', e-p)))
            return -1;
        x++;
        p++;
    } while (x<4);

    if (!(s = memchr(p, ':', e-p)))
        return -1;

    *(t = (char*)s) = '\0';
    s++;
    /* token is now at 'token', address is at 'p', and
     * port is at 's' */
    mbc.slave.sin_family = AF_INET;
    mbc.slave.sin_port = htons(atoi(s));
    mbc.slave.sin_addr.s_addr = inet_addr(p);
    mbc.sock = socket(AF_INET, SOCK_STREAM, 0);

    *t = '-';

    if (connect(mbc.sock, (struct sockaddr*)&mbc.slave, sizeof(struct sockaddr_in)) != 0) {
#ifdef DEBUG
        syslog(LOG_ERR, "connect() failed: %s", strerror(errno));
#endif
        return -1;
    }

    memcpy(out, "AUTH ", 5);
    memcpy(out+5, token, 19);
    *(out+24) = '\n';
    send(mbc.sock, out, 25, 0);

    return 0;
}

/** 
 * Connect to the master, return 0 on success
 **/
int
mbc_master_connect()
{
    if ((mbc.sock = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
        syslog(LOG_ERR, "socket() failed: %s\n", strerror(errno));
    else {
        mbc.master.sin_family      = AF_INET;
        mbc.master.sin_port        = htons(5304);
        mbc.master.sin_addr.s_addr = inet_addr(master);

        return connect(mbc.sock, (struct sockaddr*)&mbc.master, sizeof(struct sockaddr_in));
    }

    return mbc.sock;
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
mbc_master_send_login()
{
    send(mbc.sock, "AUTH client test test\n", strlen("AUTH client test test\n"), 0);

    return 0;
}

