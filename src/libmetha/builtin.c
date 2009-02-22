/*-
 * builtin.c
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "errors.h"
#include "ftpparse.h"
#include "worker.h"
#include "urlengine.h"
#include "io.h"
#include "builtin.h"

/** 
 * Builtin parsers except for the html parser which is in html.c
 **/

struct {
    const char *name;
    int         len;
} protocols[] = {
    {"http", 4},
    {"ftp", 3},
};

/** 
 * Default CSS parser
 **/
M_CODE
lm_parser_css(worker_t *w, iobuf_t *buf, uehandle_t *ue_h, url_t *url)
{
    return lm_extract_css_urls(ue_h, buf->ptr, buf->sz);
}

/** 
 * Parse the given string as CSS and add the found URLs to
 * the uehandle.
 **/
M_CODE
lm_extract_css_urls(uehandle_t *ue_h, char *p, size_t sz)
{
    char *e = p+sz;
    char *t, *s;
    while (p = memmem(p, e-p, "url", 3)) {
        p += 3;
        while (isspace(*p)) p++;
        if (*p == '(') {
            do p++; while (isspace(*p));
            t = (*p == '"' ? "\")"
                : (*p == '\'' ? "')" : ")"));
            if (*t != ')')
                p++;
        } else 
            t = (*p == '"' ? "\""
                : (*p == '\'' ? "'" : ";"));
        if (!(s = memmem(p, e-p, t, strlen(t))))
            continue;

        ue_add(ue_h, p, s-p);
        p = s;
    }

    return M_OK;
}

/** 
 * Default plaintext parser
 **/
M_CODE
lm_parser_text(worker_t *w, iobuf_t *buf, uehandle_t *ue_h, url_t *url)
{
    return lm_extract_text_urls(ue_h, buf->ptr, buf->sz);
}

M_CODE
lm_extract_text_urls(uehandle_t *ue_h, char *p, size_t sz)
{
    int x;
    char *s, *e = p+sz;

    for (p = strstr(p, "://"); p && p<e; p = strstr(p+1, "://")) {
        for (x=0;x<2;x++) {
            if (p-e >= protocols[x].len 
                && strncmp(p-protocols[x].len, protocols[x].name, protocols[x].len) == 0) {
                for (s=p+3; s < e; s++) {
                    if (!isalnum(*s) && *s != '%' && *s != '?'
                        && *s != '=' && *s != '&' && *s != '/'
                        && *s != '.') {
                        ue_add(ue_h, p-protocols[x].len, (s-p)+protocols[x].len);
                        break;
                    }
                }
                p = s;
            }
        }
    }

    return M_OK;
}

/** 
 * Default FTP parser. Expects data returned from the default
 * FTP handler.
 **/
M_CODE
lm_parser_ftp(worker_t *w, iobuf_t *buf, uehandle_t *ue_h, url_t *url)
{
    char *p, *prev;
    struct ftpparse info;
    char name[128]; /* i'm pretty sure no filename will be longer than 127 chars... */
    int  len;

    for (prev = p = buf->ptr; p<buf->ptr+buf->sz; p++) {
        if (*p == '\n') {
            if (p-prev) {
                if (ftpparse(&info, prev, p-prev)) {
                    if (info.namelen >= 126) {
                        lm_warning("file name too long\n");
                        continue;
                    }
                    if (info.flagtrycwd) {
                        memcpy(name, info.name, info.namelen);
                        name[info.namelen] = '/';
                        name[info.namelen+1] = '\0';
                        len = info.namelen+1;
                    } else {
                        strncpy(name, info.name, info.namelen);
                        len = info.namelen;
                    }

                    ue_add(ue_h, name, len);
                }
                prev = p+1;
            } else
                prev = p+1;
        }
    }

    return M_OK;
}

