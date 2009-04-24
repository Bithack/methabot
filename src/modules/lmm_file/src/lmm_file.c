/*-
 * lmm_file.c
 * This file is part of lmm_file
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
 * http://bithack.se/projects/methabot/modules/lmm_file/
 */

/** 
 * lmm_file - Javascript-File API for libmetha
 **/

#include "js.h"
#include <metha/metha.h>

static M_CODE lmm_file_init(metha_t *m);
static M_CODE lmm_file_uninit(metha_t *m);
static M_CODE lmm_file_prepare(metha_t *m);

lm_mod_properties =
{
    .name      = "lmm_file",
    .version   = "0.2.0",
    .init      = &lmm_file_init,
    .uninit    = &lmm_file_uninit,
    .prepare   = &lmm_file_prepare,

    .num_parsers = 0,
    .parsers = {
    },
};

static M_CODE
lmm_file_init(metha_t *m)
{
    /* register global javascript functions */
    lmetha_register_jsfunction(m, "fopen",    &lmm_file_open,     2);
    lmetha_register_jsfunction(m, "fwrite",   &lmm_file_write,    2);
    lmetha_register_jsfunction(m, "fread",    &lmm_file_read,     1);
    lmetha_register_jsfunction(m, "fclose",   &lmm_file_close,    2);
    lmetha_register_jsfunction(m, "fremove",  &lmm_file_remove,   1);
    lmetha_register_jsfunction(m, "opendir",  &lmm_file_opendir,  1);
    lmetha_register_jsfunction(m, "readdir",  &lmm_file_readdir,  1);
    lmetha_register_jsfunction(m, "closedir", &lmm_file_closedir, 1);

    return M_OK;
}

static M_CODE
lmm_file_prepare(metha_t *m)
{
    return M_OK;
}

static M_CODE
lmm_file_uninit(metha_t *m)
{
    return M_OK;
}

