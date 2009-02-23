/*-
 * js.c
 * This file is part of lmm_mysql
 *
 * Copyright (C) 2009, Emil Romanus <emil.romanus@gmail.com>
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
 * http://bithack.se/projects/methabot/modules/lmm_mysql/
 */

#include <stdint.h>
#include <pthread.h>
#include <mysql.h>
#include "js.h"
#include "../../libmetha/module.h"

/*pthread_mutex_t lmm_query_lock;*/

extern char    *host;
extern uint32_t port;
extern char    *user;
extern char    *pwd;
extern char    *db;
extern char    *sock;

JSBool lmm_MySQLClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_real_escape_string(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_query(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_fetch_assoc(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_num_rows(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_affected_rows(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_insert_id(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_mysql_error(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret);
static JSBool lmm_MySQLClass_finalize(JSContext *cx, JSObject *this);
static JSBool lmm_MySQLResultClass_finalize(JSContext *cx, JSObject *this);

struct JSClass MySQLClass = {
    "MySQL",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, lmm_MySQLClass_finalize,
    0, 0, 0, lmm_MySQLClass_construct, 0, 0, 0, 0
};

struct JSClass MySQLResultClass = {
    "MySQLResult",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, lmm_MySQLResultClass_finalize,
    0, 0, 0, 0, 0, 0, 0, 0
};

JSFunctionSpec lmm_MySQLClass_functions[] = {
    {"query",         lmm_mysql_query,         1},
    {"affected_rows", lmm_mysql_affected_rows, 0},
    {"insert_id",     lmm_mysql_insert_id,     0},
    {"real_escape_string", lmm_mysql_real_escape_string, 1},
    {"error",         lmm_mysql_error, 0},
    0
};

JSFunctionSpec lmm_MySQLResultClass_functions[] = {
    {"fetch_assoc", lmm_mysql_fetch_assoc, 0},
    {"num_rows",    lmm_mysql_num_rows,    0},
    0
};

/** 
 * Constructor for MySQL objects.
 *
 * Currently this only uses the statically set connection information.
 **/
JSBool
lmm_MySQLClass_construct(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL *my;

    if (!(my = mysql_init(0))) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    if (!user) user = strdup("root");

    lm_message("lmm_mysql: connecting: '%s'@'%s'...\n", user, (!host?"localhost":host));

    if (!mysql_real_connect(my, host, user, pwd, db, port, sock, 0)) {
        lm_error("lmm_mysql: failed: %s\n", mysql_error(my));
        return JS_FALSE;
    }
    lm_message("lmm_mysql: success\n");

    JS_SetPrivate(cx, this, my);
    return JS_TRUE;
}

static JSBool
lmm_MySQLClass_finalize(JSContext *cx, JSObject *this)
{
    MYSQL *my = JS_GetPrivate(cx, this);
    if (my)
        mysql_close(my);
    return JS_TRUE;
}

static JSBool
lmm_MySQLResultClass_finalize(JSContext *cx, JSObject *this)
{
    MYSQL_RES *res = JS_GetPrivate(cx, this);
    if (res)
        mysql_free_result(res);
    return JS_TRUE;
}

/** 
 * Perform the given query, and return an object of type MySQL_Result
 * or false.
 **/
static JSBool
lmm_mysql_query(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL      *my  = JS_GetPrivate(cx, this);
    MYSQL_RES  *res = 0;
    const char *q = JS_GetStringBytes(JSVAL_TO_STRING(*argv));
    JSObject   *r;

    if (argc < 1)
        return JS_FALSE;

    if (mysql_real_query(my, q,
                JS_GetStringLength(JSVAL_TO_STRING(*argv))) != 0) {
        /* an error occured */
        *ret = JSVAL_FALSE;
        return JS_TRUE;
    }

    if (!(r = JS_NewObject(cx, &MySQLResultClass, 0, 0)))
        return JS_FALSE;

    /** 
     * Even if mysql_store_result fails or is an empty set,
     * we must return a MySQLResultClass to differ 
     * success from fail.
     **/
    if ((res = mysql_store_result(my)))
        JS_SetPrivate(cx, r, res);

    *ret = OBJECT_TO_JSVAL(r);

    return JS_TRUE;
}

static JSBool
lmm_mysql_affected_rows(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL *my = JS_GetPrivate(cx, this);

    if (!my) {
        return JS_FALSE;
    }

    *ret = INT_TO_JSVAL(mysql_affected_rows(my));

    return JS_TRUE;
}

/**
 * Return a string representation of the last error
 **/
static JSBool
lmm_mysql_error(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL      *my = JS_GetPrivate(cx, this);
    const char *err;
    JSString   *s;

    if (!my)
        return JS_FALSE;

    err = mysql_error(my);
    s = JS_NewStringCopyN(cx, err, strlen(err));
    *ret = STRING_TO_JSVAL(s);

    return JS_TRUE;
}

static JSBool
lmm_mysql_insert_id(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL *my = JS_GetPrivate(cx, this);

    if (!my) {
        return JS_FALSE;
    }

    *ret = INT_TO_JSVAL(mysql_insert_id(my));

    return JS_TRUE;
}

static JSBool
lmm_mysql_real_escape_string(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL       *my;
    int          len;
    const char *from;
    char *to;
    JSString   *res;

    if (!(my = JS_GetPrivate(cx, this)))
        return JS_FALSE;

    if (argc < 1)
        return JS_FALSE;

    len = JS_GetStringLength(JSVAL_TO_STRING(*argv));

    if (!(to = malloc((len*2)+1)))
        return JS_FALSE;

    from = JS_GetStringBytes(JSVAL_TO_STRING(*argv));

    len = mysql_real_escape_string(my, to, from, len);
    res = JS_NewStringCopyN(cx, to, len);

    *ret = STRING_TO_JSVAL(res);

    free(to);
    return JS_TRUE;
}

/** 
 * Fetch one row from the result set and put the column values
 * in an associative array, with the column names as keys.
 **/
static JSBool
lmm_mysql_fetch_assoc(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL_FIELD   *fields;
    MYSQL_ROW     *row;
    JSObject      *out;
    MYSQL_RES     *res;
    unsigned int   x;
    unsigned int   num_fields;
    unsigned long *lengths;

    if (!(res = JS_GetPrivate(cx, this)))
        return JS_FALSE;

    if (!(row = mysql_fetch_row(res)))
        *ret = JSVAL_FALSE;
    else {
        /** 
         * Create an ordinary javascript object, this will
         * act as an associative array.
         **/
        out        = JS_NewObject(cx, 0, 0, 0);
        fields     = mysql_fetch_fields(res);
        lengths    = mysql_fetch_lengths(res);
        num_fields = mysql_num_fields(res);
        jsval v;

        for (x=0; x<num_fields; x++) {
            if (row[x]) {
                v = STRING_TO_JSVAL(
                        JS_NewStringCopyN(cx, row[x],
                                          (size_t)lengths[x])
                        );
            } else
                v = JSVAL_NULL;
            JS_DefineProperty(cx, out, fields[x].name,
                              v, 0, 0, 0);
        }

        *ret = OBJECT_TO_JSVAL(out);
    }
    
    return JS_TRUE;
}

static JSBool
lmm_mysql_num_rows(JSContext *cx, JSObject *this, uintN argc, jsval *argv, jsval *ret)
{
    MYSQL_RES     *res;
    if (!(res = JS_GetPrivate(cx, this)))
        return JS_FALSE;

    *ret = INT_TO_JSVAL(mysql_num_rows(res));
    
    return JS_TRUE;
}

