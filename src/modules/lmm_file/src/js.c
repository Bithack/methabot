/*-
 * js.c
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

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <metha/metha.h>
#include "js.h"

JSBool lmm_FileClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_FileClass_finalize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_file_open(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_file_write(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_file_close(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_file_read(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_file_remove(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);

struct JSClass FileClass = {
    "FileHandle",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, lmm_FileClass_finalize,
    0, 0, 0, lmm_FileClass_construct, 0, 0, 0, 0
};

JSFunctionSpec lmm_FileClass_functions[] = {
    {"open",        lmm_file_open,      2},
    {"write",       lmm_file_write,     2},
    {"close",       lmm_file_close,     1},
    {"read",        lmm_file_read,      2},
    {"remove",      lmm_file_remove,    1},
    0
};

static JSBool
lmm_file_open(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 2) {
        lm_error("not enough arguments for file.open(), %d given\n", argc);
        return JS_FALSE;
    }

    const char *filename = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    const char *mode = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
    if (!mode || !filename) return JS_FALSE;

    FILE *fh;
    fh = fopen(filename, mode);

    *ret = OBJECT_TO_JSVAL(fh);
    return JS_TRUE;
}

static JSBool
lmm_file_write(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 2) {
        lm_error("not enough arguments for file.write(fh, text), %d given\n", argc);
        return JS_FALSE;
    }

    FILE *fh = JSVAL_TO_OBJECT(argv[0]);
    const char *text = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
    if (!fh || !text)
        return JS_FALSE;

    fprintf(fh, text);

    return JS_TRUE;
}

static JSBool
lmm_file_close(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 1) {
        lm_error("not enough arguments for file.close(fh), %d given\n", argc);
        return JS_FALSE;
    }

    FILE *fh = JSVAL_TO_OBJECT(argv[0]);
    if (!fh)
        return JS_FALSE;

    fclose(fh);

    return JS_TRUE;
}

static JSBool
lmm_file_read(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 2) {
        lm_error("not enough arguments for file.read(fh, length), %d given\n", argc);
        return JS_FALSE;
    }

    char *buffer;
    size_t res;
    FILE *fh = JSVAL_TO_OBJECT(argv[0]);
    const long filesize = (long)JSVAL_TO_INT(argv[1]);

    if (!fh)
        return JS_FALSE;

    if (!(buffer = malloc(filesize+1))) {
        lm_error("out of mem\n");
        return JS_FALSE;
    }

    res = fread(buffer, 1, filesize, fh);

    if (res != filesize) {
        lm_error("unable to read file\n");
        return JS_FALSE;
    }

    JSString *tmp = JS_NewStringCopyN(cx, buffer, (filesize>0?filesize-1:filesize));
    if (tmp == NULL) {
        lm_error("out of mem\n");
        return JS_FALSE;
    }

    *ret = STRING_TO_JSVAL(tmp);
    free(buffer);
    return JS_TRUE;
}

static JSBool
lmm_file_remove(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 1) {
        lm_error("not enough arguments for file.remove(filename), %d given\n", argc);
        return JS_FALSE;
    }

    const char *filename = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (!filename)
        return JS_FALSE;

    if (remove(filename) != 0) {
        /* return errno in case of error? */
        *ret = INT_TO_JSVAL(0);
    } else {
        *ret = INT_TO_JSVAL(1);
    }

    return JS_TRUE;
}

JSBool
lmm_FileClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    return JS_TRUE;
}

JSBool
lmm_FileClass_finalize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    return JS_TRUE;
}

