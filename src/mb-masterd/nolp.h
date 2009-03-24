/*-
 * nolp.h
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

#ifndef _NOLP__H_
#define _NOLP__H_

#define NOLP_DEFAULT_BUFSZ 256

enum {
    NOLP_CMD,
    NOLP_EXPECT,
    NOLP_LINE,
};

struct nolp_fn {
    const char *name;
    int (*cb)(void*, char *buf, int size);
};

typedef struct nolp {
    char *buf;
    int   state;
    int   sz;
    int   cap;
    int   fd;
    void *private;
    int (*next_cb)(void*, char *buf, int size);
    struct nolp_fn *fn;
} nolp_t;

int     nolp_expect(nolp_t *no, int size, int (*complete_cb)(void*, char *, int));
void    nolp_free(nolp_t *no);
nolp_t* nolp_create(struct nolp_fn *fn, int sock);
int     nolp_recv(nolp_t *no);

#endif