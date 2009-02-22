/*-
 * metha.h
 * This file is part of libmetha
 *
 * Copyright (C) 2008, Emil Romanus <emil.romanus@gmail.com>
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

#ifndef _METHA__H_
#define _METHA__H_

#include <stdarg.h>
#include <pthread.h>
#include <jsapi.h>

#include "filetype.h"
#include "crawler.h"
#include "errors.h"
#include "io.h"
#include "url.h"
#include "wfunction.h"
#include "events.h"

typedef enum {
    LMOPT_MODE,
    LMOPT_NUM_THREADS,
    LMOPT_INITIAL_CRAWLER,
    LMOPT_ENABLE_BUILTIN_PARSERS,
    LMOPT_IO_VERBOSE,
    LMOPT_USERAGENT,
    LMOPT_PRIMARY_SCRIPT_DIR,
    LMOPT_SECONDARY_SCRIPT_DIR,
    LMOPT_PRIMARY_CONF_DIR,
    LMOPT_SECONDARY_CONF_DIR,
    LMOPT_FILE_HANDLER,
    LMOPT_PROXY,
    LMOPT_ENABLE_COOKIES,
    LMOPT_MODULE_DIR,
} LMOPT;

typedef enum {
    LMOPT_GLOBAL_MESSAGE_FUNC,
    LMOPT_GLOBAL_WARNING_FUNC,
    LMOPT_GLOBAL_ERROR_FUNC,
} LMOPT_GLOBAL;

struct observer_pool {
    unsigned int     count;
    struct observer *o;
};

struct observer {
    void *arg2;
    M_CODE (*callback)(void *, void *);
};

struct worker_object {
    const char *name;
    JSClass    *class;
};

struct script_desc {
    char *full;
    char *name;
    JSScript   *script;
};

typedef struct metha {
    io_t        io;
    ue_t        ue;

    /** 
     * The event loop and its events
     **/
    struct {
        struct ev_loop  *loop;
        struct ev_async all_empty;
    } ev;

    /* lmetha_exec_once() and lmetha_exec_provided() will save 
     * their results here, so we can pick it up the next time
     * any exec function is called */
    uehandle_t *ueh_save;

    /* worker (threads) locks */
    int w_message_id;
    struct {crawler_t *cr; uehandle_t *ue_h;} w_message_ret;

    pthread_mutex_t  w_lk_reply;
    pthread_rwlock_t w_lk_num_waiting;
    volatile int     w_num_waiting;
    struct worker  **waiting_queue;

    int crawler; /* initial crawler */

    filetype_t     **filetypes;
    crawler_t      **crawlers;
    int              num_filetypes;
    int              num_crawlers;

    char           **configs;
    int              num_configs;

    wfunction_t    **functions;
    int              num_functions;

    struct worker   **workers;
    int               nworkers;

    struct lm_mod  **modules;
    int              num_modules;

    struct script_desc *scripts;
    int                 num_scripts;

    JSRuntime        *e4x_rt;
    JSContext        *e4x_cx;
    JSObject         *e4x_global;

    struct lm_scope  *scopes;
    int               num_scopes;

    /* primary and secondary script dir */
    const char       *script_dir1;
    const char       *script_dir2;

    const char       *conf_dir1;
    const char       *conf_dir2;

    const char       *module_dir;

    struct observer_pool  observer_pool[LM_EV_COUNT];
    struct worker_object *worker_objs;
    int                   num_worker_objs;

    M_CODE (*handler)(void *, const url_t *);
    void    *private;

    int builtin_parsers;
    int num_threads;

    int robotstxt; /* is robots.txt support enabled in ANY crawler? */
    int prepared;
} metha_t;

/* metha.c */
M_CODE   lmetha_global_setopt(LMOPT_GLOBAL opt, ...);
metha_t *lmetha_create(void);
void     lmetha_destroy(metha_t *m);
M_CODE   lmetha_prepare(metha_t *m);
M_CODE   lmetha_exec(metha_t *m, int argc, const char **argv); 
M_CODE   lmetha_exec_provided(metha_t *m, const char *base_url, const char *buf, size_t len);
M_CODE   lmetha_setopt(metha_t *m, LMOPT opt, ...);
M_CODE   lmetha_add_wfunction(metha_t *m, const char *name, uint8_t type, uint8_t purpose, void *fun);
M_CODE   lmetha_add_filetype(metha_t *m, filetype_t *ft);
filetype_t* lmetha_get_filetype(metha_t *m, const char *name);
crawler_t*  lmetha_get_crawler(metha_t *m, const char *name);
M_CODE   lmetha_register_worker_object(metha_t *m, const char *name, JSClass *class);
M_CODE   lmetha_init_jsclass(metha_t *m, JSClass *class, JSNative constructor, uintN nargs, JSPropertySpec *ps, JSFunctionSpec *fs, JSPropertySpec *static_ps, JSFunctionSpec *static_fs);

    /* io.c */
M_CODE lm_iothr_launch(io_t *io);
M_CODE lm_init_io(io_t *io, metha_t *m);
void   lm_uninit_io(io_t *io);
M_CODE lm_get(struct crawler *c, url_t *url, iostat_t *stat);
M_CODE lm_peek(struct crawler *c, url_t *url, iostat_t *stat);
M_CODE lm_multipeek_add(iohandle_t *ioh, url_t *url, int id);
ioprivate_t* lm_multipeek_wait(iohandle_t *ioh);

#endif
