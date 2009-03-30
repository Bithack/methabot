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

#include "client.h"
#include "nolp.h"

static int mbc_slave_on_start(nolp_t *no, char *buf, int size);
static int mbc_slave_on_stop(nolp_t *no, char *buf, int size);
static int mbc_slave_on_continue(nolp_t *no, char *buf, int size);
static int mbc_slave_on_pause(nolp_t *no, char *buf, int size);
static int mbc_slave_on_exit(nolp_t *no, char *buf, int size);

/* commands received from the slave, to this client */
struct nolp_fn sl_commands[] = {
    {"START", &mbc_slave_on_start},
    {"STOP", &mbc_slave_on_stop},
    {"CONTINUE", &mbc_slave_on_continue},
    {"PAUSE", &mbc_slave_on_pause},
    {"EXIT", &mbc_slave_on_exit},
    {0}
};

static int
mbc_slave_on_start(nolp_t *no, char *buf, int size)
{
    lmetha_wakeup_worker(mbc.m, "default", "http://bithack.se/");
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

