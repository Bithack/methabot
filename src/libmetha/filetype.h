/*-
 * filetype.h
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

#ifndef _LM_FILETYPE__H_
#define _LM_FILETYPE__H_

#include <stdint.h>
#include "errors.h"
#include "wfunction.h"
#include "umex.h"

#define FT_FLAG_HAS_PARSER   1
#define FT_FLAG_HAS_HANDLER  2

#define FT_FLAG_ISSET(ft, x) ((ft)->flags & (x))
#define FT_ID_NULL ((FT_ID)-1)

typedef uint8_t FT_ID;

typedef struct parser_chain {
    wfunction_t   **parsers;
    unsigned int    num_parsers;
} parser_chain_t;

typedef struct filetype {
    char    *name;
    uint8_t  id; /* this is set up by lmetha_prepare() */

    /** 
     * extensions, mimetypes and attributes are represented 
     * as arrays in configuration files. Their set-functions
     * (lm_filetype_*_set()) expect a pointer array.
     * XXX: Note that the *_set() functions will TAKE the 
     *      array given, and not make a copy of it.
     **/
    char   **extensions;
    int      e_count;
    char   **mimetypes;
    int      m_count;
    char   **attributes;
    int      attr_count;

    umex_t *expr;

    /* parser_str identifies the parser_chain, and will
     * contain the full string as given to the 'parser'
     * filetype option in configuration files. */
    char                *parser_str;
    parser_chain_t       parser_chain;

    union {
        char           *name;
        const struct crawler *ptr;
    } switch_to;

    union {
        wfunction_t *wf;
        char        *name;
    } handler;

    uint8_t flags;
} filetype_t;

filetype_t *lm_filetype_create(const char *name, uint32_t nlen);
void    lm_filetype_destroy(filetype_t *ft);
M_CODE  lm_filetype_add_extension(filetype_t *ft, const char *name);
M_CODE  lm_filetype_add_mimetype(filetype_t *ft, const char *name);
M_CODE  lm_filetype_set_extensions(filetype_t *ft, char **extensions, int num_extensions);
M_CODE  lm_filetype_set_attributes(filetype_t *ft, char **attributes, int num_attributes);
M_CODE  lm_filetype_set_mimetypes(filetype_t *ft, char **mimetypes, int num_mimetypes);
M_CODE  lm_filetype_set_expr(filetype_t *ft, const char *expr);
void    lm_filetype_clear(filetype_t *ft);
M_CODE  lm_filetype_dup(filetype_t *dest, filetype_t *source);

#endif

