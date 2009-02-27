/*-
 * conn.h
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

#ifndef _MBM_CONN__H_
#define _MBM_CONN__H_

#include "../libmetha/libev/ev.h"

enum {
    CONN_ACTION_NONE,
    CONN_ACTION_SEND_CONFIG,
};

struct conn {
    int   sock;
    int   auth;
    int   authenticated;
    int   action;
    ev_io fd_ev;
};

enum {
    MBM_AUTH_TYPE_CLIENT = 0,
    MBM_AUTH_TYPE_SLAVE,
    MBM_AUTH_TYPE_STATUS,
    MBM_AUTH_TYPE_OPERATOR,

    NUM_AUTH_TYPES,
};

void mbm_ev_conn_accept(EV_P_ ev_io *w, int revents);

#endif
