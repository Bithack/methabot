/*-
 * lmm_mysql.c
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

/** 
 * lmm_mysql - Javascript-MySQL API for libmetha
 **/

#include "js.h"
#include "../../libmetha/module.h"

char    *host = 0;
uint32_t port = 0;
char    *user = 0;
char    *sock = 0;
char    *pwd  = 0;
char    *db   = 0;

static M_CODE lmm_mysql_init(metha_t *m);
static M_CODE lmm_mysql_uninit(metha_t *m);
static M_CODE lmm_mysql_prepare(metha_t *m);

lm_mod_properties =
{
    .name      = "lmm_mysql",
    .version   = "0.1.0",
    .init      = &lmm_mysql_init,
    .uninit    = &lmm_mysql_uninit,
    .prepare   = &lmm_mysql_prepare,

    .num_parsers = 0,
    .parsers = {
    },
};

/** 
 * Scope options for the scope 'mysql'
 **/
#define NUM_SCOPE_VARS (sizeof(mysql_scope_vars)/sizeof(struct lm_scope_opt))
struct lm_scope_opt mysql_scope_vars[] = {
    LM_DEFINE_SCOPE_VAR("server", LM_VAL_T_STRING, &host),
    LM_DEFINE_SCOPE_VAR("port",   LM_VAL_T_UINT,   &port),
    LM_DEFINE_SCOPE_VAR("user",   LM_VAL_T_STRING, &user),
    LM_DEFINE_SCOPE_VAR("password",LM_VAL_T_STRING, &pwd),
    LM_DEFINE_SCOPE_VAR("sock",LM_VAL_T_STRING, &sock),
    LM_DEFINE_SCOPE_VAR("database",LM_VAL_T_STRING, &db),
};

static M_CODE
lmm_mysql_init(metha_t *m)
{
    lmetha_register_scope(m, "mysql", &mysql_scope_vars, NUM_SCOPE_VARS);

    return M_OK;
}

/** 
 * We should now have server and login information, so we 
 * connect to the server and store the mysql object avaiable 
 * to workers.
 **/
static M_CODE
lmm_mysql_prepare(metha_t *m)
{
    lmetha_init_jsclass(m, &MySQLClass, lmm_MySQLClass_construct, 0, 0, &lmm_MySQLClass_functions, 0, 0);
    lmetha_init_jsclass(m, &MySQLResultClass, 0, 0, 0, &lmm_MySQLResultClass_functions, 0, 0);
    lmetha_register_worker_object(m, "mysql", &MySQLClass);

    return M_OK;
}

static M_CODE
lmm_mysql_uninit(metha_t *m)
{
    if (host)
        free(host);
    if (user)
        free(user);
    if (pwd)
        free(pwd);
    if (db)
        free(db);
    if (sock)
        free(sock);
    return M_OK;
}

