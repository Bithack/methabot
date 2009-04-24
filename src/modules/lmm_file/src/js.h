/*-
 * js.h
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


#ifndef _LMM_FILE_JS__H_
#define _LMM_FILE_JS__H_

#include <jsapi.h>

JSBool lmm_file_open(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_write(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_close(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_read(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_remove(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_opendir(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_readdir(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
JSBool lmm_file_closedir(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);

#endif
