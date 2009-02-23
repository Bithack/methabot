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
#include "../../libmetha/module.h"

static M_CODE lmm_file_init(metha_t *m);
static M_CODE lmm_file_uninit(metha_t *m);
static M_CODE lmm_file_prepare(metha_t *m);

lm_mod_properties =
{
    .name      = "lmm_file",
    .version   = "0.1.0",
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
    return M_OK;
}

static M_CODE
lmm_file_prepare(metha_t *m)
{
    lmetha_init_jsclass(m, &FileClass, 0, 0, 0, &lmm_FileClass_functions, 0, 0);
    lmetha_register_worker_object(m, "file", &FileClass);

    return M_OK;
}

static M_CODE
lmm_file_uninit(metha_t *m)
{
    return M_OK;
}

