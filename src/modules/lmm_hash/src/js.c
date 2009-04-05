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
#include <pthread.h>
#include <metha/metha.h>
#include "js.h"
#include "md5.h"

JSBool lmm_HashClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_HashClass_finalize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_hash_md5(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);

struct JSClass HashClass = {
    "FileHandle",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, lmm_HashClass_finalize,
    0, 0, 0, lmm_HashClass_construct, 0, 0, 0, 0
};

JSFunctionSpec lmm_HashClass_functions[] = {
    {"md5",        lmm_hash_md5,      1},
    0
};

static JSBool
lmm_hash_md5(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    if (argc < 1) {
//        LM_ERROR("not enough arguments for hash.md5()\n");
        return JS_FALSE;
    }

    const char *string = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (!string) return JS_FALSE;

    struct MD5Context md5_cx;

    MD5Init(&md5_cx);
    printf("%s\n", string);
    MD5Update(&md5_cx, string, strlen(string));
    printf("%s\n", string);

    //*ret = OBJECT_TO_JSVAL(fh);
    return JS_TRUE;
}

JSBool
lmm_HashClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    return JS_TRUE;
}

JSBool
lmm_HashClass_finalize(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    return JS_TRUE;
}

