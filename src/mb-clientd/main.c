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

#include <ev.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "client.h"
#include "../libmetha/metha.h"
#include "../libmetha/worker.h"
#include "../mn-masterd/daemon.h"

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
void mbc_ev_idle(EV_P_ ev_async *w, int revents);
static void mbc_ev_sig(EV_P_ ev_signal *w, int revents);

static void mbc_lm_status_cb(metha_t *m, worker_t *w, url_t *url);
static void mbc_lm_target_cb(metha_t *m, worker_t *w, url_t *url, attr_list_t *al, filetype_t *ft);
static void mbc_lm_error_cb(metha_t *m, const char *s, ...);
static void mbc_lm_warning_cb(metha_t *m, const char *s, ...);
static void mbc_lm_ev_cb(metha_t *m, int ev);
static int mbc_ev_slave_login(nolp_t *no, char *buf, int size);

struct mbc mbc = {
    .state = MBC_STATE_DISCONNECTED,
};

extern struct nolp_fn sl_commands[];

char *master_host     = 0;
unsigned master_port     = 0;
char *master_username = 0;
char *master_password = 0;
char *user = 0;
char *group = 0;

extern char *arg;

struct lmc_scope client_scope =
{
    "client",
    {
        LMC_OPT_STRING("master_host", &master_host),
        LMC_OPT_UINT("master_port", &master_port),
        LMC_OPT_STRING("master_username", &master_username),
        LMC_OPT_STRING("master_password", &master_password),
        LMC_OPT_STRING("user", &user),
        LMC_OPT_STRING("group", &group),
        LMC_OPT_END,
    }
};

int
main(int argc, char **argv)
{
    M_CODE          r;
    ev_signal sigint_listen;
    ev_signal sigterm_listen;
    ev_signal sighup_listen;
    lmc_parser_t *lmc;

    signal(SIGPIPE, SIG_IGN);
    openlog("mb-clientd", 0, 0);
    syslog(LOG_INFO, "started");

    lmetha_global_init();

    if (!(mbc.m = lmetha_create()))
        exit(1);

    if (!(mbc.no = nolp_create(&sl_commands, 0)))
        exit(1);

    lmetha_setopt(mbc.m, LMOPT_TARGET_FUNCTION, mbc_lm_target_cb);
    lmetha_setopt(mbc.m, LMOPT_STATUS_FUNCTION, mbc_lm_status_cb);
    lmetha_setopt(mbc.m, LMOPT_ERROR_FUNCTION, mbc_lm_error_cb);
    lmetha_setopt(mbc.m, LMOPT_WARNING_FUNCTION, mbc_lm_warning_cb);
    lmetha_setopt(mbc.m, LMOPT_EV_FUNCTION, mbc_lm_ev_cb);
    lmetha_setopt(mbc.m, LMOPT_ENABLE_BUILTIN_PARSERS, 1);
    lmetha_setopt(mbc.m, LMOPT_ENABLE_COOKIES, 1);
    lmetha_setopt(mbc.m, LMOPT_PRIMARY_SCRIPT_DIR, "/usr/share/metha/scripts");

    mbc.loop = ev_default_loop(0);

    lmc = lmc_create(0);
    lmc_add_scope(lmc, &client_scope);
    if (lmc_parse_file(lmc, "/etc/mb-clientd.conf") != M_OK) {
        fprintf(stderr, "%s\n", lmc->last_error);
        return 1;
    }

    /* catch signals */
    ev_signal_init(&sigint_listen, &mbc_ev_sig, SIGINT);
    ev_signal_init(&sigterm_listen, &mbc_ev_sig, SIGTERM);
    ev_signal_init(&sighup_listen, &mbc_ev_sig, SIGHUP);

    ev_io_init(&mbc.sock_ev, mbc_ev_master, mbc.sock, EV_READ);
    ev_timer_init(&mbc.timer_ev, mbc_ev_timer, TIMER_WAIT, .0f);
    mbc.timer_ev.repeat = TIMER_WAIT;
    ev_async_init(&mbc.idle_ev, mbc_ev_idle);

    if (mbc_master_connect() == 0) {
        mbc_master_send_login();
        mbc.state = MBC_STATE_WAIT_LOGIN;
        mbc_set_active(mbc.loop, MBC_MASTER);
    } else {
        ev_timer_start(mbc.loop, &mbc.timer_ev);
        syslog(LOG_INFO, "master is away, retrying in 5 secs");
    }

    ev_async_start(mbc.loop, &mbc.idle_ev);
    ev_signal_start(mbc.loop, &sigint_listen);
    ev_signal_start(mbc.loop, &sigterm_listen);
    ev_signal_start(mbc.loop, &sighup_listen);
    ev_loop(mbc.loop, 0);

    ev_default_destroy();
    lmetha_destroy(mbc.m);
    lmetha_global_cleanup();
    return 0;
}

/** 
 * Callback from libmetha when a new url has been crawled
 **/
static void
mbc_lm_status_cb(metha_t *m, worker_t *w, url_t *url)
{
    char buf[512];
    char *p;
    int  len;

    /* prefer using the stacked buffer, but if it is not
     * big enough, then malloc one on the heap and use
     * that one instead. Most likely URLs wont be longer
     * than 512 chars */
    if (url->sz >= 507) {
        if (!(p = malloc(url->sz+6)))
            syslog(LOG_ERR, "out of mem");
    } else
        p = buf;
    len = sprintf(p, "URL %s\n", url->str);
    send(mbc.sock, p, len, 0);

    if (p != buf)
        free(p);
}

/** 
 * Callback from libmetha when a target has been found
 **/
static void
mbc_lm_target_cb(metha_t *m, worker_t *w,
                 url_t *url, attr_list_t *attributes,
                 filetype_t *ft)
{
    char pre[11+1+url->sz+96];
    int  sz;
    int  x;
    int  total = 0;
    int y;
    for (x=0; x<attributes->num_attributes; x++) {
        sz = sprintf(pre, "%d", attributes->list[x].size);
        char *s = strchr(attributes->list[x].name, ' ');
        if (!s) y = strlen(attributes->list[x].name);
        else y = s-attributes->list[x].name;

        total += y+attributes->list[x].size+2+sz;
    }
    sz = sprintf(pre, "TARGET 0 %s %.64s %d\n",
            url->str, ft->name,
            total);
    send(mbc.sock, pre, sz, 0);
    for (x=0; x<attributes->num_attributes; x++) {
        char *s = strchr(attributes->list[x].name, ' ');
        if (!s) y = strlen(attributes->list[x].name);
        else y = s-attributes->list[x].name;
        sz = sprintf(pre, "%.*s %d ", y, attributes->list[x].name, attributes->list[x].size);
        send(mbc.sock, pre, sz, 0);
        send(mbc.sock, attributes->list[x].value, attributes->list[x].size, 0);
    }
}

static void
mbc_lm_error_cb(metha_t *m, const char *s, ...)
{
    syslog(LOG_ERR, "error: %s", s);
}

static void
mbc_lm_warning_cb(metha_t *m, const char *s, ...)
{
    syslog(LOG_WARNING, "warning: %s", s);
}

static void
mbc_lm_ev_cb(metha_t *m, int ev)
{
    switch (ev) {
        case LM_EV_IDLE:
            /**
             * The crawling session is idle, we signal our own event handler
             * to request a new URL from the slave.
             **/
            ev_async_send(mbc.loop, &mbc.idle_ev);
            /* also stop the thread laucnhed by lmetha_exec_async() */
            lmetha_signal(m, LM_SIGNAL_EXIT);
            break;
    }
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
             * will reconnect to the master after 5 secs */
            mbc.state = MBC_STATE_DISCONNECTED;
            ev_timer_start(mbc.loop, &mbc.timer_ev);
            break;

        case MBC_MASTER:
            ev_io_init(&mbc.sock_ev, mbc_ev_master, mbc.sock, EV_READ);
            ev_io_start(EV_A_ &mbc.sock_ev);
            break;

        case MBC_SLAVE:
            nolp_expect_line(mbc.no, &mbc_ev_slave_login);
            ev_io_init(&mbc.sock_ev, mbc_ev_slave, mbc.sock, EV_READ);
            ev_io_start(EV_A_ &mbc.sock_ev);
            break;
    }
}

/** 
 * called when timer was reached. the timer should only
 * be started if we are waiting to reconnect to the master
 **/
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

/** 
 * crawling session is done, we're out of URLs
 * and must notify the slave and then wait for
 * new URL(s)
 **/
void
mbc_ev_idle(EV_P_ ev_async *w, int revents)
{
    char buf[128];
    int x;
    int sz;

    /* loop through all filetypes and report their
     * counters so the server can calculate statistics */
    for (x=0; x<mbc.m->num_filetypes; x++) {
        sz = sprintf(buf, "COUNT %.64s %u\n",
                mbc.m->filetypes[x]->name,
                mbc.m->filetypes[x]->counter);
        lm_filetype_counter_reset(mbc.m->filetypes[x]);
        send(mbc.sock, buf, sz, 0);
    }

    send(mbc.sock, "STATUS 0\n", 9, 0);
    /* wait for our libmetha thread to exit before
     * continuing with the event loop */
    lmetha_wait(mbc.m);
    free(arg);
    arg = 0;
    mbc.state = MBC_STATE_STOPPED;
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
        syslog(LOG_ERR, "master has gone away");
        fail = 1;
    } else {
        switch (mbc.state) {
            case MBC_STATE_WAIT_LOGIN:
                /** 
                 * Reply from master after login.
                 **/
                if ((msg = atoi(buf)) == 100) {
                    syslog(LOG_INFO, "logged on to master, waiting for token");
                    mbc.state = MBC_STATE_WAIT_TOKEN;
                } else {
                    syslog(LOG_INFO,
                            "logging in to master failed, code %d",
                            atoi(buf));
                    fail = 1;
                }
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
        close(w->fd);
        mbc_set_active(mbc.loop, MBC_NONE);
    }
}

/** 
 * when we receive a reply from the slave after 
 * an AUTH command
 **/
static int
mbc_ev_slave_login(nolp_t *no, char *buf, int size)
{
    if (atoi(buf) == 100) {
        syslog(LOG_INFO, "slave connection established");
        mbc.state = MBC_STATE_STOPPED;
    } else {
        syslog(LOG_WARNING, "slave login failed");
        return -1;
    }

    return 0;
}

/** 
 * Called when a message from the slave is sent
 **/
void
mbc_ev_slave(EV_P_ ev_io *w, int revents)
{
    mbc.no->fd = w->fd;
    if (nolp_recv(mbc.no) != 0) {
        syslog(LOG_INFO, "connection to slave lost");
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
 * xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx-a.b.c.d:Z
 * \______________________________________/ \_____/ |
 *                   |                        /     |
 *            Login token (SHA1)          IP-Addr Port
 * 
 * mbc.sock must be closed before this function is 
 * called, i.e. the master must have been disconnected.
 * The socket is reused.
 **/
int
mbc_slave_connect(const char *token, int len)
{
    char       *p, *e, *s, *t;
    char        out[100];

    if (len < 48)
        return -1;
    e = (p = token)+len;
    p += 41;
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
        syslog(LOG_ERR, "connect() failed: %s", strerror(errno));
        return -1;
    }

    sprintf(out, "AUTH %.40s\n", token);
    send(mbc.sock, out, 46, 0);
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
        mbc.master.sin_port        = htons(master_port);
        mbc.master.sin_addr.s_addr = inet_addr(master_host);

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
    char buf[256];
    int  sz;

    sz = sprintf(buf, "AUTH client %.64s %.64s\n", 
            master_username ? master_username : "default",
            master_password ? master_password : "default");

    if (send(mbc.sock, buf, sz, 0) <= 0)
        return -1;

    return 0;
}

static void
mbc_ev_sig(EV_P_ ev_signal *w, int revents)
{
    syslog(LOG_INFO, "interrupted, exiting");
    lmetha_signal(mbc.m, LM_SIGNAL_EXIT);
    lmetha_wait(mbc.m);
    ev_unloop(EV_A_ EVUNLOOP_ONE);
}

