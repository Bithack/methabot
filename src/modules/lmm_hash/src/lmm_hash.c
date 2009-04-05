/*-
 * lmm_hash.c
 * This file is part of lmm_hash
 *
 * Copyright (C) 2009, Rasmus Karlsson <pajlada@bithack.se>
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
 * http://bithack.se/projects/methabot/modules/lmm_hash/
 */

/** 
 * lmm_hash - Javascript-File API for libmetha
 **/

#include "js.h"
#include <metha/metha.h>

static M_CODE lmm_hash_init(metha_t *m);
static M_CODE lmm_hash_uninit(metha_t *m);
static M_CODE lmm_hash_prepare(metha_t *m);

lm_mod_properties =
{
    .name      = "lmm_hash",
    .version   = "0.1.0",
    .init      = &lmm_hash_init,
    .uninit    = &lmm_hash_uninit,
    .prepare   = &lmm_hash_prepare,

    .num_parsers = 0,
    .parsers = {
    },
};

static M_CODE
lmm_hash_init(metha_t *m)
{
    return M_OK;
}

static M_CODE
lmm_hash_prepare(metha_t *m)
{
    lmetha_init_jsclass(m, &HashClass, 0, 0, 0, &lmm_HashClass_functions, 0, 0);
    lmetha_register_worker_object(m, "hash", &HashClass);

    LM_ERROR(m, "help");

    return M_OK;
}

static M_CODE
lmm_hash_uninit(metha_t *m)
{
    return M_OK;
}

