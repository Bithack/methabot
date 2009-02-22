/*-
 * metha.c
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

#include "default.h"
#include "metha.h"
#include "worker.h"
#include "js.h"
#include "events.h"
#include "libev/ev.h"
#include "mod.h"
#include "builtin.h"

#include <jsapi.h>
#include <stdio.h>
#include <string.h>

static M_CODE lm_prepare_filetypes(metha_t *m);
static M_CODE lm_prepare_crawlers(metha_t *m);
static M_CODE lm_default_handler(void *unused, const url_t *url);
static M_CODE lm_exec_once(metha_t *m, iohandle_t *io_h, uehandle_t *ue_h);
static M_CODE lm_call_init(metha_t *m, uehandle_t *h, int argc, const char **argv);
static struct script_desc* lm_load_script(metha_t *m, const char *file);
static wfunction_t *lm_str_to_wfunction(metha_t *m, const char *name, uint8_t purpose);

static JSClass global_jsclass = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static struct {
    const char *ident;
    unsigned int timer_wait;
    unsigned int timer_wait_mp;
} timer_vals[] = {
    {"aggressive", 0, 0},
    {"friendly",  10, 2},
    {"coward",    30, 5},
};

static wfunction_t
m_builtin_parsers[] = {
    {
        .type    = LM_WFUNCTION_TYPE_NATIVE,
        .purpose = LM_WFUNCTION_PURPOSE_PARSER,
        .name = "html",
        .fn.native_parser = &lm_parser_html
    }, {
        .type    = LM_WFUNCTION_TYPE_NATIVE,
        .purpose = LM_WFUNCTION_PURPOSE_PARSER,
        .name = "text",
        .fn.native_parser = &lm_parser_text
    }, {
        .type    = LM_WFUNCTION_TYPE_NATIVE,
        .purpose = LM_WFUNCTION_PURPOSE_PARSER,
        .name = "ftp",
        .fn.native_parser = &lm_parser_ftp
    }, {
        .type    = LM_WFUNCTION_TYPE_NATIVE,
        .purpose = LM_WFUNCTION_PURPOSE_PARSER,
        .name = "xmlconv",
        .fn.native_parser = &lm_parser_xmlconv
    }, {
        .type    = LM_WFUNCTION_TYPE_NATIVE,
        .purpose = LM_WFUNCTION_PURPOSE_PARSER,
        .name = "css",
        .fn.native_parser = &lm_parser_css
    },
};

/** 
 * This function allows setting the global error/message/warning reporting functions.
 **/
M_CODE
lmetha_global_setopt(LMOPT_GLOBAL opt, ...)
{
    va_list ap;
    va_start(ap, opt);

    switch (opt) {
        case LMOPT_GLOBAL_MESSAGE_FUNC:
            lm_message = va_arg(ap, void*);
            break;
        case LMOPT_GLOBAL_WARNING_FUNC:
            lm_warning = va_arg(ap, void*);
            break;
        case LMOPT_GLOBAL_ERROR_FUNC:
            lm_error = va_arg(ap, void*);
            break;
    }

    va_end(ap);
    return M_OK;
}

/** 
 * LMOPTs are defined in metha.h
 **/
M_CODE
lmetha_setopt(metha_t *m, LMOPT opt, ...)
{
    va_list ap;
    int x;
    char *arg;

    va_start(ap, opt);

    switch (opt) {
        case LMOPT_MODE:
            arg = va_arg(ap, char *);
            for (x=0; x<3; x++) {
                if (strcasecmp(arg, timer_vals[x].ident) == 0) {
                    if (timer_vals[x].timer_wait) {
                        m->io.synchronous = 1;
                        m->io.timer_wait = timer_vals[x].timer_wait*CLOCKS_PER_SEC;
                        m->io.timer_wait_mp = timer_vals[x].timer_wait_mp*CLOCKS_PER_SEC;
                        m->io.timer_last = (clock_t)-1;
                    } else
                        m->io.synchronous = 0;
                    break;
                }
            }
            if (x == 3)
                goto badarg;
            break;

        case LMOPT_IO_VERBOSE:
            m->io.verbose = va_arg(ap, int);
            break;

        case LMOPT_ENABLE_BUILTIN_PARSERS:
            if (m->builtin_parsers)
                break;
            m->builtin_parsers = 1;
            for (x=0; x<LM_NUM_BUILTIN_PARSERS; x++)
                lmetha_add_wfunction(m, m_builtin_parsers[x].name, m_builtin_parsers[x].type, LM_WFUNCTION_PURPOSE_PARSER, m_builtin_parsers[x].fn.native_parser);
            break;

        case LMOPT_NUM_THREADS:
#ifdef WIN32
            m->num_threads = 1;
            break;
#endif
            if (!(m->num_threads = va_arg(ap, int))) {
                lm_error("thread count must be 1 or greater\n");
                goto badarg;
            }
            break;

        case LMOPT_INITIAL_CRAWLER:
            arg = va_arg(ap, char*);
            for (x=0; x<m->num_crawlers; x++) {
                if (strcmp(m->crawlers[x]->name, arg) == 0) {
                    m->crawler = x;
                    goto success;
                }
            }
            lm_error("could not find crawler '%s'\n", arg);
            goto fail;

        case LMOPT_USERAGENT:
            m->io.user_agent = va_arg(ap, char*);
            break;

        case LMOPT_PROXY:
            m->io.proxy = va_arg(ap, char*);
            break;

        case LMOPT_PRIMARY_SCRIPT_DIR:
            m->script_dir1 = va_arg(ap, const char*);
            m->script_dir1 = strdup(m->script_dir1);
            break;

        case LMOPT_MODULE_DIR:
            m->module_dir = va_arg(ap, const char*);
            m->module_dir = strdup(m->module_dir);
            break;

        case LMOPT_SECONDARY_SCRIPT_DIR:
            m->script_dir2 = strdup(va_arg(ap, char*));
            break;

        case LMOPT_PRIMARY_CONF_DIR:
            m->conf_dir1 = strdup(va_arg(ap, char*));
            break;

        case LMOPT_SECONDARY_CONF_DIR:
            m->conf_dir2 = strdup(va_arg(ap, char*));
            break;

        case LMOPT_FILE_HANDLER:
            m->handler = va_arg(ap, void*);
            break;

        case LMOPT_ENABLE_COOKIES:
            m->io.cookies = va_arg(ap, int);
            break;

        default:
            lm_error("unknown option (%d)\n", opt);
            goto badopt;
    }

success:
    va_end(ap);
    return M_OK;

fail:
    va_end(ap);
    return M_FAILED;

badopt:
    va_end(ap);
    return M_BAD_OPTION;

badarg:
    va_end(ap);
    return M_BAD_VALUE;
}

/** 
 * allocate a metha object
 **/
metha_t *
lmetha_create(void)
{
    metha_t *m = calloc(1, sizeof(metha_t));

#ifdef WIN32
    lm_warning("running on win32, asynchronous mode disabled\n");
#endif

    /** 
     * Initialize all pthread mutexes and conditions
     * TODO: do proper cleanup on failure
     **/
    if (pthread_rwlock_init(&m->w_lk_num_waiting, 0) != 0)
        return 0;
    if (pthread_mutex_init(&m->w_lk_reply, 0) != 0)
        return 0;

    if (ue_init(&m->ue) != M_OK)
        return 0;

    if (!(m->e4x_rt = JS_NewRuntime(8L*1024L*1024L))) {
        lmetha_destroy(m);
        return 0;
    }

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) Initializing JS engine (%s)\n", m, JS_GetImplementationVersion());
#endif

    /** 
     * Set up the "main" context. This context will only be used for 
     * garbage collection and loading scripts.
     **/
    if (!(m->e4x_cx = JS_NewContext(m->e4x_rt, 8192))) {
        lm_error("could not create a javascript context");
        return 0;
    }

    JS_SetErrorReporter(m->e4x_cx, &lm_jserror);

    if (!(m->e4x_global = JS_NewObject(m->e4x_cx, &global_jsclass, 0, 0))) {
        lm_error("could not create the global javascript object");
        return 0;
    }

    JS_SetGlobalObject(m->e4x_cx, m->e4x_global);
    JS_InitStandardClasses(m->e4x_cx, m->e4x_global);

    m->w_num_waiting = 0;

    return m;
}

/** 
 * free everything, clean up
 **/
void
lmetha_destroy(metha_t *m)
{
    int x;

    /* stop the IO-thread */
    lm_iothr_stop(&m->io);

    for (x=0; x<LM_EV_COUNT; x++) {
        struct observer_pool *pool = &m->observer_pool[x];
        if (pool->o)
            free(pool->o);
    }

    if (m->num_filetypes) {
        for (x=0; x<m->num_filetypes; x++)
            lm_filetype_destroy(m->filetypes[x]);
        free(m->filetypes);
    }

    if (m->num_crawlers) {
        for (x=0; x<m->num_crawlers; x++)
            lm_crawler_destroy(m->crawlers[x]);
        free(m->crawlers);
    }

    if (m->num_scripts) {
        for (x=0; x<m->num_scripts; x++) {
            free(m->scripts[x].full);
            JS_DestroyScript(m->e4x_cx, m->scripts[x].script);
        }
        free(m->scripts);
    }

    if (m->worker_objs)
        free(m->worker_objs);

    if (m->e4x_cx)
        JS_DestroyContext(m->e4x_cx);

    if (m->num_modules) {
        for (x=0; x<m->num_modules; x++)
            lm_mod_unload(m, m->modules[x]);
        free(m->modules);
    }

    if (m->num_configs) {
        for (x=0; x<m->num_configs; x++)
            free(m->configs[x]);
        free(m->configs);
    }

    if (m->nworkers)
        free(m->workers);

    if (m->num_functions) {
        for (x=0; x<m->num_functions; x++) {
            /* XXX: proper cleanup */
            free(m->functions[x]);
        }
        free(m->functions);
    }

    if (m->script_dir1)
        free(m->script_dir1);

    if (m->script_dir2)
        free(m->script_dir2);

    if (m->conf_dir1)
        free(m->conf_dir1);

    if (m->conf_dir2)
        free(m->conf_dir2);

    if (m->module_dir)
        free(m->module_dir);

    if (m->e4x_rt) 
        JS_DestroyRuntime(m->e4x_rt);

    JS_ShutDown();

    /** 
     * Destroy all pthread mutexes and conditions
     **/
    pthread_rwlock_destroy(&m->w_lk_num_waiting);
    pthread_mutex_destroy(&m->w_lk_reply);

    ue_uninit(&m->ue);
    lm_uninit_io(&m->io);

	free(m);
}

filetype_t*
lmetha_get_filetype(metha_t *m, const char *name)
{
    int x;
    filetype_t *ret = 0;

    for (x=0; x<m->num_filetypes; x++) {
        if (strcasecmp(m->filetypes[x]->name, name) == 0) {
            ret = m->filetypes[x];
            break;
        }
    }

    return ret;
}

crawler_t*
lmetha_get_crawler(metha_t *m, const char *name)
{
    int x;
    crawler_t *ret = 0;

    for (x=0; x<m->num_crawlers; x++) {
        if (strcasecmp(m->crawlers[x]->name, name) == 0) {
            ret = m->crawlers[x];
            break;
        }
    }

    return ret;
}

/**
 * Execute once on the given URL
 *
 * TODO: look if we have a m->ueh_save first
 **/
M_CODE
lmetha_exec_once(metha_t *m, const char *url)
{
    M_CODE ret;

    if (!m->prepared)
        return M_NOT_READY;

    iohandle_t *io_h = lm_iohandle_obtain(&m->io);
    uehandle_t *ue_h = ue_handle_obtain(&m->ue);

    ue_add_initial(ue_h, url, strlen(url));

    ret = lm_exec_once(m, io_h, ue_h);

    lm_iohandle_destroy(io_h);

    m->ueh_save = ue_h;
    return ret;
}

/** 
 * Works like lmetha_exec_once() but on a provided buffer in memory. 
 *
 * TODO: look if we have a m->ueh_save first
 **/
M_CODE
lmetha_exec_provided(metha_t *m, const char *base_url,
                     const char *buf, size_t len)
{
    M_CODE ret;

    if (!m->prepared)
        return M_NOT_READY;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) provided buffer (size: %d b)\n", m, (int)len);
#endif

    uehandle_t *ue_h = ue_handle_obtain(&m->ue);
    iohandle_t *io_h = lm_iohandle_obtain(&m->io);

    lm_io_provide(io_h, buf, len);
    ue_add_initial(ue_h, base_url, strlen(base_url));

    ret = lm_exec_once(m, io_h, ue_h);

    m->ueh_save = ue_h;
    return ret;
}

static M_CODE
lm_exec_once(metha_t *m, iohandle_t *io_h, uehandle_t *ue_h)
{
#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) running once with crawler '%s'\n", m, m->crawlers[m->crawler]->name);
#endif

    worker_t *w = malloc(sizeof(worker_t));
    int x;

    if (!w)
        return M_OUT_OF_MEM;

    w->m = m;
    w->io_h = io_h;
    w->ue_h = ue_h;
    w->argc = 0;
    w->argv = 0;

    lm_worker_set_crawler(w, m->crawlers[m->crawler]);
    lm_worker_run_once(w);

    lm_worker_free(w);
    return M_OK;
}

/** 
 * Start crawling session. The array of char-pointers at m->urls 
 * will be converted to URLs and used as initial urls.
 **/
M_CODE 
lmetha_exec(metha_t *m, int argc, const char **argv)
{
    int x, y;

    if (!m->prepared)
        return M_NOT_READY;

    if (!(m->ev.loop = ev_loop_new(EVFLAG_AUTO)))
        return M_EVENT_ERROR;

    if (!(m->waiting_queue = malloc(m->num_threads*sizeof(void*))))
        return M_OUT_OF_MEM;

    m->w_num_waiting = 0;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) starting with crawler '%s'\n", m, m->crawlers[m->crawler]->name);
#endif

    ev_async_init(&m->ev.all_empty, &lm_event_workers_all_empty);
    m->ev.all_empty.data = m;
    ev_async_start(m->ev.loop, &m->ev.all_empty);

    /* Spread the initial urls (from m->urls), one to each worker. 
     * If there are less workers than there are initial URLs, then 
     * the last worker will receive all remaining URLs. */
    for (x=0; x<m->num_threads; x++) {
        uehandle_t *h;

        /** 
         * We might have a saved uehandle from earlier, if 
         * lmetha_exec_once() or lmetha_exec_provided() was 
         * called.
         **/
        if (m->ueh_save) {
            h = m->ueh_save;
            m->ueh_save = 0;
        } else {
            h = ue_handle_obtain(&m->ue);
        }

        if (x < argc) {
            if (x == m->num_threads-1) {
                /* give the last worker the rest of the URLs */
                if (lm_fork_worker(m, m->crawlers[m->crawler], h, argc-x, argv+x) != M_OK)
                    return M_FAILED;
            } else
                if (lm_fork_worker(m, m->crawlers[m->crawler], h, 1, argv+x) != M_OK)
                    return M_FAILED;
            continue;
        }
        if (lm_fork_worker(m, m->crawlers[m->crawler], h, 0, 0) != M_OK)
            return M_FAILED;
        
    }

    /* start the event loop, waiting for signals from workers */
    ev_loop(m->ev.loop, 0);

    for (x=0; x<m->w_num_waiting; x++) {
        worker_t *w = m->waiting_queue[x];
        pthread_mutex_lock(&w->lock);
        w->message = LM_WORKER_MSG_STOP;
        pthread_cond_signal(&w->wakeup_cond);
        pthread_mutex_unlock(&w->lock);
    }

    /* Wait for all threads to exit and clean up */
    for (x=0; x<m->nworkers; x++) {
        pthread_join(m->workers[x]->thr, 0);
    }

    return M_OK;
}

/** 
 * Initialize a JSClass in the current runtime
 **/
M_CODE
lmetha_init_jsclass(metha_t *m, JSClass *class, JSNative constructor,
                    uintN nargs, JSPropertySpec *ps, JSFunctionSpec *fs,
                    JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    if (!(JS_InitClass(m->e4x_cx, m->e4x_global, 0, class, constructor,
                       nargs, ps, fs, static_ps, static_fs)))
        return M_FAILED;

    return M_OK;
}

/** 
 * Register a data field to the 'this' object of ALL workers. This 
 * is useful to modules such as lmm_mysql which puts a mysql object
 * available to all the workers.
 *
 * This function will create a new object of the given class, and thus
 * call the class' constructor. The resulting object will be sent to the 
 * workers when they are created, and put in this.<name>.
 **/
M_CODE
lmetha_register_worker_object(metha_t *m, const char *name, JSClass *class)
{
    if (!(m->worker_objs = realloc(m->worker_objs, (m->num_worker_objs+1)*sizeof(struct worker_object))))
        return M_OUT_OF_MEM;

    m->worker_objs[m->num_worker_objs].name = name;
    m->worker_objs[m->num_worker_objs].class = class;
    m->num_worker_objs ++;

    return M_OK;
}

/** 
 * Prepare for execution:
 * - Set up crawlers and their respective filetypes.
 * - TODO: Report warnings about unused filetypes.
 * - Make sure multiple filetypes don't share the same name.
 * - Set up file extension and mimetype index (ftindex) for each crawler.
 * - Start the I/O thread for concurrent MIME lookups
 * - Match filetype parser strings with real parser callbacks and bind them
 * - Run all loaded modules prepare functions
 **/
M_CODE
lmetha_prepare(metha_t *m)
{
#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) preparing %d crawler(s) and %d filetype(s)\n", m, m->num_crawlers, m->num_filetypes); 
#endif
    M_CODE ret;

    /* start up the IO-thread */
    if (lm_init_io(&m->io, m) != M_OK)
        return M_FAILED;
    if (lm_iothr_launch(&m->io) != M_OK)
        return M_FAILED;
    
    if (!m->num_crawlers)
        return M_NO_CRAWLERS;
    if (!m->num_filetypes)
        return M_NO_FILETYPES;

    /* Make sure no more than 1 worker thread is launched if running synchronously */
    if (m->io.synchronous) {
        if (m->num_threads != 1)
            lm_warning("worker count must be 1 when running with timer\n");
        m->num_threads = 1;
    }

    if (!m->io.user_agent)
        m->io.user_agent = LM_DEFAULT_USERAGENT;

    if (!m->handler)
        m->handler = &lm_default_handler;

    if ((ret = lm_prepare_filetypes(m)) != M_OK)
        return ret;

    if ((ret = lm_prepare_crawlers(m)) != M_OK)
        return ret;

    /** 
     * Prepare the loaded modules by calling their prepare functions
     **/
    int x;
    for (x=0; x<m->num_modules; x++) {
        if (m->modules[x]->props->prepare) {
            if (m->modules[x]->props->prepare(m) != M_OK) {
                return M_FAILED;
            }
        }
    }

    m->prepared = 1;

    return M_OK;
}

/** 
 * Prepare crawlers. Build filetype-indexes, make sure no name is 
 * used twice, and so on.
 **/
static M_CODE
lm_prepare_crawlers(metha_t *m)
{
    int        x, y, z;
    crawler_t *cr;
    char      *p;

    /* Check crawler names */
    for (x=0; x<m->num_crawlers; x++) {
        for (y=x+1; y<m->num_crawlers; y++) {
            if (strcmp(m->crawlers[x]->name, m->crawlers[y]->name) == 0) {
                lm_error("multiple definitions of crawler '%s'\n", m->crawlers[x]->name);
                return M_CR_MULTIDEF;
            }
        }
    }

    /* create lookup-indexes for each crawler to its specific filetypes */
    for (x=0; x<m->num_crawlers; x++) {
        /* generate a list of pointers to all related filetypes, note that this
         * list will be sent to the ftindex, and will be freed when the ftindex
         * is destroyed */
        filetype_t **ft_list;
        cr = m->crawlers[x];

        int sz  = 0; /* ft_list count */

        if (cr->num_filetypes == 0) {
            /* if this crawler does not have its own list of filetypes,
             * ALL defined filetypes will be added */
            if (!(ft_list = malloc(m->num_filetypes*sizeof(void*))))
                return M_OUT_OF_MEM;

            for (z=0; z<m->num_filetypes; z++)
                ft_list[z] = m->filetypes[z];

            sz = m->num_filetypes;
        } else {
            int cap = 8;
            if (!(ft_list = malloc(8*sizeof(void*))))
                return M_OUT_OF_MEM;
            /* loop through this crawler's list of filetypes, and add 
             * them to an array which we will use later when generating
             * this crawler's filetype index */
            for (y=0; y<cr->num_filetypes; y++) {
                for (z=0; z<m->num_filetypes; z++) {
                    if (strcmp(m->filetypes[z]->name, cr->filetypes[y]) == 0) {
                        /* matched, copy the pointer */
                        ft_list[sz] = m->filetypes[z];
                        sz++;
                        if (sz >= cap) {
                            if (!(ft_list = realloc(ft_list, (cap*2)*sizeof(void*))))
                                return M_OUT_OF_MEM;
                            cap*=2;
                        }
                        break;
                    }
                }
            }
            /* justify allocation-overhead */
            ft_list = realloc(ft_list, sz*sizeof(void*));
        }
        /* give the list to the ftindex, and let it generate an index of 
         * all the filetypes */
        lm_ftindex_generate(&cr->ftindex, sz, ft_list);

        /* if this crawler has a peek limit above 0, we must set the 
         * LM_CRFLAG_EPEEK flag so other functions will be able to 
         * quickly determine whether this crawler allows external 
         * peeking */
        if (cr->peek_limit)
            lm_crawler_flag_set(cr, LM_CRFLAG_EPEEK);

        /* if any of our crawlers have enabled robots.txt support, we
         * must set the robotstxt element of the metha_t object to 1
         * so that robots.txt files will be downloaded by default. */
        if (lm_crawler_flag_isset(cr, LM_CRFLAG_ROBOTSTXT))
            m->robotstxt = 1;

        /* this crawler might have its own init function, and if
         * so it might require a javascript file to be loaded */
        if (cr->init) {
            if (!(p = strchr(cr->init, '/'))) {
                lm_error("module init functions are currently disabled\n");
            } else {
                *p = '\0';
                for (y=0;; y++) {
                    if (y == m->num_scripts) {
                        lm_load_script(m, cr->init);
                        break;
                    }
                    if (strcmp(m->scripts[y].name, cr->init) == 0)
                        break;
                }
                *p = '/';
            }
        }

        /* if this crawler has an initial type, set the initial_filetype.ptr to
         * the filetype_t pointer */
        if (cr->initial_filetype.name) {
            for (y=0;; y++) {
                if (y == m->num_filetypes) {
                    lm_error("could not find filetype '%s' set as initial type for crawler '%s'\n",
                                cr->initial_filetype.name, cr->name);
                    return M_UNKNOWN_FILETYPE;
                }
                if (strcmp(m->filetypes[y]->name, cr->initial_filetype.name) == 0) {
                    free(cr->initial_filetype.name);
                    cr->initial_filetype.ptr = m->filetypes[y];
                    break;
                }
            }
        }

        if (cr->default_handler.name) {
            wfunction_t *wf = lm_str_to_wfunction(m, cr->default_handler.name, LM_WFUNCTION_PURPOSE_HANDLER);
            if (!wf)
                return M_FAILED;
            free(cr->default_handler.name);
            cr->default_handler.wf = wf;
        }

        /* now we must fix dynamic_url and friends of crawler_t, 
         * see comments in ftindex.h */
        void **friends[5] = {&cr->ftindex.dynamic_url,
                             &cr->ftindex.extless_url,
                             &cr->ftindex.dir_url,
                             &cr->ftindex.unknown_url,
                             &cr->ftindex.ftp_dir_url};
        char *fnames[5]   = {"dynamic-url", "extless-url",
                             "dir-url",     "unknown-url",
                             "ftp_dir_url"};

        for (y=0; y<5; y++) {
            if (!*friends[y])
                continue;

            char *value = (char*)*(friends[y]);

            if (*value == '@') {
                cr->ftindex.flags |= (LM_FTIFLAG_BIND_DYNAMIC_URL << y);
                /* find the filetype */
                int found = 0;
                for (z=0; z<m->num_filetypes; z++) {
                    if (strcmp(value+1, m->filetypes[z]->name) == 0) {
                        *friends[y] = m->filetypes[z];
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    lm_error("could not find filetype '%s' requested by '%s' in crawler '%s'\n",
                                value+1, fnames[y], cr->name);
                    return M_UNKNOWN_FILETYPE;
                }
            } else {
                /* either "lookup" or "discard" */
                if (strcmp(value, "lookup") == 0)
                    *(friends[y]) = 0;
                else if (strcmp(value, "discard") == 0)
                    *(friends[y]) = 1;
                else {
                    /* The variable was set to something not recognized,
                     * not a filetype and neither 'lookup' nor 'discard'
                     * i guess we should do nothing but bail out with an 
                     * error */
                    lm_error("unrecognized value '%s' set for '%s' in crawler '%s'\n",
                                value, fnames[y], cr->name);
                    return M_FAILED;
                }
            }
        }

        lm_crawler_flag_set(cr, LM_CRFLAG_PREPARED);
    }
    return M_OK;
}

/** 
 * Prepare the filetypes in the metha_t object. Give them each an 
 * ID and find and verify their parsers.
 **/
static M_CODE
lm_prepare_filetypes(metha_t *m)
{
    int         x, y;
    filetype_t  *ft;

    for (x=0; x<m->num_filetypes; x++) {
        ft = m->filetypes[x];

        /* Make sure no other filetype uses the same name */
        for (y=x+1; y<m->num_filetypes; y++) {
            if (strcmp(ft->name, m->filetypes[y]->name) == 0) {
                lm_error("multiple definitions of filetype '%s'\n", ft->name);
                return M_FT_MULTIDEF;
            }
        }

        if (ft->switch_to.name) {
            char *n = ft->switch_to.name; 
            ft->switch_to.name = 0;
            for (y=0; y<m->num_crawlers; y++) {
                if (strcmp(m->crawlers[y]->name, n) == 0) {
                    ft->switch_to.ptr = m->crawlers[y];
                    break;
                }
            }
            if (!ft->switch_to.ptr) {
                lm_error("unknown crawler '%s' requested by filetype '%s'\n", n, ft->name);
                free(n);
                return M_UNKNOWN_CRAWLER;
            }

            free(n);
        }

        /* give this filetype an 8-bit ID, so we can bind URLs to it
         * the ID is simply it's position in the array +1 */
        ft->id = x+1;
        if (ft->parser_str) {
            /* the parser string will contain a list of parsers
             * separated by comma. We will generate a parser chain
             * that chains all the specified parsers */
            char        *p;
            wfunction_t *wf;
            
            if ((p = strtok(ft->parser_str, " \n\t,"))) {
                do {
                    /* the current parser in the list is now null-
                     * terminated and pointed to by p */
                    if (!(wf = lm_str_to_wfunction(m, p, LM_WFUNCTION_PURPOSE_PARSER))
                            || lm_filetype_add_parser(ft, wf) != M_OK)
                        return M_FAILED;
                } while ((p = strtok(0, " \n\t,")));
            }
        }

        if (ft->handler.name) {
            wfunction_t *wf = lm_str_to_wfunction(m, ft->handler.name, LM_WFUNCTION_PURPOSE_HANDLER);
            if (!wf)
                return M_FAILED;
            free(ft->handler.name);
            ft->handler.wf = wf;
        }
    }

    return M_OK;
}

/** 
 * Find an already added wfunction by looking at the given string. 
 * If the function identified by the string haven't been added before,
 * add it and load the relevant files (i.e. javascript files).
 *
 * 'purpose' specifies the purpose for which this wfunction will be
 * used, if the wfunction was already added and the purpose mismatches,
 * then an error will occur. This is to prevent native handlers to 
 * be called as native parsers, or vice versa (since they don't have
 * the same argument count, a segfault would probably occur).
 * 
 * Syntax of the function name:
 * If the name contains a slash ('/'), the left side is the file in
 * which the function can be found, and the right side is the name of
 * the function. For example, if we are loading the function "abc" from
 * the javascript file "test.js", 'name' should be "test.js/abc".
 *
 * XXX: this function will temporarily modify characters in 'name',
 *      so 'name' should not point to read-only memory!
 *
 * Returns 0 if the given function does not exist, or an error occured.
 **/
static wfunction_t *
lm_str_to_wfunction(metha_t *m, const char *name, uint8_t purpose)
{
    int y;
    wfunction_t *wf;
    char *s, *p = name;

    for (y=0; ; y++) {
        if (y == m->num_functions) {
            /* no matching wfun was found, and 
             * we are at the end of the list. The
             * wfun might be from a javascript file
             * that has not yet been loaded */

            if (s = strchr(p, '/')) {
                /* this is a javascript parser */
                *s = '\0';
                if (lm_load_script(m, p)) {
                    /* TODO: get the function from the JSScript object instead,
                     * lm_load_script() actually returns a pointer to a JSScript */
                    jsval func;
                    JS_GetProperty(m->e4x_cx, m->e4x_global, s+1, &func);
                    if (JS_TypeOfValue(m->e4x_cx, func) == JSTYPE_FUNCTION) {
                        *s = '/';
                        if (lmetha_add_wfunction(m, p, LM_WFUNCTION_TYPE_JAVASCRIPT, purpose, JS_ValueToFunction(m->e4x_cx, func)) != M_OK)
                            return 0;

                        return m->functions[m->num_functions-1];
                    } else {
                        *s = '/';
                        lm_error("type mismatch, parser '%s' is not a function\n", p);
                    }
                } else
                    lm_error("could not load javascript file '%s'\n", p);
            } else
                lm_error("could not find parser '%s'\n",p );

            return 0;
        }

        wf = m->functions[y];
        if (strcmp(wf->name, p) == 0) {
            /* found a matching wfunction */
            if (wf->purpose != purpose) {
                lm_error("parser/handler incompatibility\n");
                return 0;
            }
            return wf;
        }
    }

    return 0;
}

/** 
 * Add a filetype to the metha_t objects list. The filetype will 
 * be taken complete control of and freed when lmetha_destroy()
 * is called.
 **/
M_CODE
lmetha_add_filetype(metha_t *m, filetype_t *ft)
{
    if (m->num_filetypes) {
        if (!(m->filetypes = realloc(m->filetypes, (m->num_filetypes+1)*sizeof(filetype_t*))))
            return M_OUT_OF_MEM;
    } else {
        if (!(m->filetypes = malloc(sizeof(void*))))
            return M_OUT_OF_MEM;
    }

    m->filetypes[m->num_filetypes] = ft;
    m->num_filetypes ++;
    
    return M_OK;
}

/** 
 * Add a wfunction (parser, handler)
 **/
M_CODE
lmetha_add_wfunction(metha_t *m, const char *name, uint8_t type, uint8_t purpose, void *fun)
{
    va_list ap;
#ifdef DEBUG
    static const char *wfun_types[] = {
        "NATIVE",
        "JAVASCRIPT",
    };
    static const char *wfun_purposes[] = {
        "HANDLER",
        "PARSER",
    };
#endif

    if (!(m->functions = realloc(m->functions, (m->num_functions+1)*sizeof(wfunction_t*))))
        return M_OUT_OF_MEM;

    if (!(m->functions[m->num_functions] = malloc(sizeof(wfunction_t))))
        return M_OUT_OF_MEM;

    wfunction_t *wf = m->functions[m->num_functions];

    wf->type = type;
    wf->purpose = purpose;
    wf->name = strdup(name);
    wf->fn.native_parser = fun;

    m->num_functions ++;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) added wfunction '%s' [type=%s, purpose=%s]\n", m, name, wfun_types[type], wfun_purposes[purpose]);
#endif

    return M_OK;
}

/** 
 * Load the given script file into the metha object. If 'file'
 * is not an absolute path, this function will check in the 
 * directories specified by LMOPT_PRIMARY_SCRIPT_DIR and 
 * LMOPT_SECONDARY_SCRIPT_DIR.
 *
 * To load a script by relative path, the calling function must 
 * first convert it to an absolute path.
 *
 * This function will check if the script has already been loaded,
 * and if so return a pointer to it.
 **/
static struct script_desc*
lm_load_script(metha_t *m, const char *file)
{
    JSScript *js;
    jsval ret;
    char *name;
    char *full = 0;
    char *p;
    int len;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) loading script '%s'\n", m, file);
#endif

    if (*file == '/') {
        /* absolute path, find the filename itself */
        len = strlen(file);
        if (!(full = malloc(len+1)))
            return 0;
        strcpy(full, file);
    } else {
        do {
            if (m->script_dir1) {
                len = strlen(m->script_dir1)+1+strlen(file);
                full = malloc(len+1);
                strcpy(full, m->script_dir1);
                strcat(full, "/");
                strcat(full, file);
                /* have a look if the file exists in the primary script dir */
                FILE *tmp;
                if (tmp = fopen(full, "r")) {
                    fclose(tmp);
                    break;
                }
                free(full);
                if (m->script_dir2) {
                    len = strlen(m->script_dir2)+1+strlen(file);
                    full = malloc(len+1);
                    strcpy(full, m->script_dir2);
                    strcat(full, "/");
                    strcat(full, file);
                    if (tmp = fopen(full, "r")) {
                        fclose(tmp);
                        break;
                    }
                }
            }
            free(full);
            lm_error("could not find file '%s'\n", file);
            return 0;
        } while (0);
    }
    for (p=full+len; p>full; p--) {
        if (*p == '/') {
            p++;
            break;
        }
    }
    name = p;

    /* if the script, or a script with the same name, has already been loaded,
     * then return a pointer to that script*/
    int x;
    for (x=0; x<m->num_scripts; x++)
        if (strcmp(m->scripts[x].name, name) == 0)
            return &m->scripts[x];

    if (!(js = JS_CompileFile(m->e4x_cx, m->e4x_global, full))
        || JS_ExecuteScript(m->e4x_cx, m->e4x_global, js, &ret) != JS_TRUE)
        goto error;

    /* now that the script loaded successfully, add it to the script list */
    if (!(m->scripts = realloc(m->scripts, (x+1)*sizeof(struct script_desc))))
        return 0;

    m->scripts[x].full = full;
    m->scripts[x].name = name;
    m->scripts[x].script = js;
    m->num_scripts++;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) successfully loaded '%s'\n", m, file);
#endif

    return &m->scripts[x];

error:
    if (full)
        free(full);
    return 0;
}

static M_CODE
lm_default_handler(void *unused, const url_t *url)
{
    printf("[-] %s\n", url->str);
}
