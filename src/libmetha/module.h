/*-
 * module.h
 * This file is part of libmetha
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

#ifndef _LM_MODULE__H_
#define _LM_MODULE__H_

#include "wfunction.h"
#include "errors.h"

struct lm_scope_opt {
    const char       *name;
    int               type;
    union {
        M_CODE     (*set)(void*, const char *);
        M_CODE     (*array_set)(void*, char **, int);
        ptrdiff_t    offs;
        int          flag;
    } value;
};

typedef struct lm_scope {
    const char         *name;
    struct lm_scope_opt *opts;
    int           num_opts;
} lm_scope_t;

enum {
    LM_VAL_T_UINT,
    LM_VAL_T_ARRAY,
    LM_VAL_T_STRING,
    LM_VAL_T_EXTRA,
    LM_VAL_T_FLAG,
};

typedef struct lm_mod_properties {
    const char *name;
    const char *version;
    M_CODE (*init)(struct metha *);
    M_CODE (*uninit)(struct metha *);
    M_CODE (*prepare)(struct metha *);
    int         num_parsers;
    wfunction_t parsers[];
} lm_mod_properties_t;

#define lm_mod_properties lm_mod_properties_t __lm_module_properties
#define LM_DEFINE_PARSER(x, y) \
        { \
            .type = LM_WFUNCTION_TYPE_NATIVE, \
            .purpose = LM_WFUNCTION_PURPOSE_PARSER, \
            .name = x, \
            .parser.cb = y \
        }
#define LM_DEFINE_SCOPE_VAR(x, y, z) {.name = x, .type=y, .value.offs=z}

#endif
