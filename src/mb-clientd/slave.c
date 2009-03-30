/*-
 * slave.c
 * This file is part of mb-clientd 
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
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

#include "client.h"
#include "nolp.h"

static int mbc_slave_on_start(nolp_t *no, char *buf, int size);
static int mbc_slave_on_stop(nolp_t *no, char *buf, int size);
static int mbc_slave_on_continue(nolp_t *no, char *buf, int size);
static int mbc_slave_on_pause(nolp_t *no, char *buf, int size);
static int mbc_slave_on_exit(nolp_t *no, char *buf, int size);
static int mbc_slave_on_config(nolp_t *no, char *buf, int size);
static int mbc_slave_on_config_recv(nolp_t *no, char *buf, int size);

/* commands received from the slave, to this client */
struct nolp_fn sl_commands[] = {
    {"START", &mbc_slave_on_start},
    {"STOP", &mbc_slave_on_stop},
    {"CONTINUE", &mbc_slave_on_continue},
    {"PAUSE", &mbc_slave_on_pause},
    {"EXIT", &mbc_slave_on_exit},
    {"CONFIG", &mbc_slave_on_config},
    {0}
};

static int
mbc_slave_on_start(nolp_t *no, char *buf, int size)
{
    buf[size] = '\0';
#ifdef DEBUG
    syslog(LOG_DEBUG, "received url '%s' from slave", buf);
#endif
    lmetha_wakeup_worker(mbc.m, "default", buf);
    lmetha_signal(mbc.m, LM_SIGNAL_CONTINUE);

    return 0;
}

static int
mbc_slave_on_stop(nolp_t *no, char *buf, int size)
{
    return 0;
}

static int
mbc_slave_on_continue(nolp_t *no, char *buf, int size)
{
    return 0;
}

static int
mbc_slave_on_pause(nolp_t *no, char *buf, int size)
{
    return 0;
}

static int
mbc_slave_on_exit(nolp_t *no, char *buf, int size)
{
    return 0;
}

static int
mbc_slave_on_config(nolp_t *no, char *buf, int size)
{
    nolp_expect(mbc.no, atoi(buf), &mbc_slave_on_config_recv);
    return 0;
}

static int
mbc_slave_on_config_recv(nolp_t *no, char *buf, int size)
{
    static int config_read = 0;
    M_CODE r;

    if (!config_read) {
        if ((r = lmetha_read_config(mbc.m, buf, size)) != M_OK) {
            syslog(LOG_ERR, "reading libmetha config failed: %s", lm_strerror(r));
            return -1;
        }
        if ((r = lmetha_prepare(mbc.m)) != M_OK) {
            syslog(LOG_ERR, "preparing libmetha object failed: %s", lm_strerror(r));
            return -1;
        }
        if ((r = lmetha_start(mbc.m)) != M_OK) {
            syslog(LOG_ERR, "start libmetha session failed: %s", lm_strerror(r));
            return -1;
        }
    } else {
        syslog(LOG_WARNING, "can not reload configuration");
    }

    config_read = 1;

    return 0;
}
