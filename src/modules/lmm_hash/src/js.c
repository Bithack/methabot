/*-
 * js.c
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <metha/metha.h>
#include "js.h"
#include "md5.h"

static M_CODE
str_to_md5(const char *string, char *buf)
{
    if (!buf || !string) {
        return M_ERROR;
    }

    md5_state_t state;
    md5_byte_t digest[16];
    unsigned int i;

    md5_init(&state);
    md5_append(&state, (const md5_byte_t*)string, strlen(string));
    md5_finish(&state, digest);

    for (i = 0; i < 16; ++i) {
        sprintf(buf+2*i, "%02x", digest[i]);
    }

    return M_OK;
}

JSBool
lmm_hash_md5(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 1) {
        fprintf(stderr, "not enough arguments for hash.md5()\n");
        return JS_FALSE;
    }

    const char *string = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (!string) return JS_FALSE;

    char *buf = malloc(33);

    str_to_md5(string, buf);

    JSString *tmp = JS_NewStringCopyN(cx, buf, strlen(buf));
    if (tmp == NULL) {
        fprintf(stderr, "out of mem\n");
        return JS_FALSE;
    }

    *ret = STRING_TO_JSVAL(tmp);

    free(buf);
    return JS_TRUE;
}

JSBool
lmm_hash_md5_file(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 1) {
        fprintf(stderr, "not enough arguments for md5()\n");
        return JS_FALSE;
    }

    const char *filename = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (!filename) return JS_FALSE;

    FILE  *fh;
    char  *string;
    char  *buf = malloc(33);
    long   filesize;
    size_t res;
    
    fh = fopen(filename, "r");
    if (fh == NULL) {
        fprintf(stderr, "unable to open file\n");
        return JS_FALSE;
    }

    fseek(fh, 0, SEEK_END);
    filesize = ftell(fh);
    rewind(fh);

    string = malloc(filesize);
    if (!string) {
        fprintf(stderr, "out of mem\n");
        return JS_FALSE;
    }

    res = fread(string, 1, filesize, fh);
    if (res != filesize) {
        fprintf(stderr, "reading error\n");
        fclose(fh);
        return JS_FALSE;
    }
    *(string+filesize-1) = '\0';

    str_to_md5(string, buf);

    JSString *tmp = JS_NewStringCopyN(cx, buf, strlen(buf));
    if (tmp == NULL) {
        fprintf(stderr, "out of mem\n");
        return JS_FALSE;
    }

    *ret = STRING_TO_JSVAL(tmp);

    free(buf);
    return JS_TRUE;
}

