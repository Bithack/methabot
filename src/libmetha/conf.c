/*-
 * conf.c
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "errors.h"
#include "metha.h"
#include "filetype.h"
#include "crawler.h"
#include "conf.h"
#include "mod.h"

enum {
    STATE_ROOT,
    STATE_PRE_NAME,
    STATE_NAME,
    STATE_POST_NAME,
    STATE_END_NAME,
    STATE_PRE_OBJ,
    STATE_OBJ,
    STATE_PRE_VAL,
    STATE_VAL,
    STATE_POST_VAL,
    STATE_ARRAY_VAL,
    STATE_ARRAY_PRE_VAL,
    STATE_DIRECTIVE_ARG,
};

static const char *val_str[] =
{
    "LM_VAL_T_UINT",
    "LM_VAL_T_ARRAY",
    "LM_VAL_T_STRING",
    "LM_VAL_T_EXTRA",
    "LM_VAL_T_FLAG",
};

struct object {
    const char         *name;
    const struct lm_scope_opt *opts;
    const int           num_opts;
    void               *(*find)(metha_t *, const char *);
    void               *(*reset)(void *);
    void               *(*copy)(void *, void *);
    void               *(*constructor)(const char *, uint32_t);
    void                (*destructor)(void*);
    uintptr_t           m_offs; /* offset to an array in the metha_t struct where to store this obj */
    uintptr_t           m_count_offs; /* offset to the object count in the metha_t */
    uintptr_t           flags_offs; /* offset to a child element where all flags should be set */
    int                 size; /* size per object */
};

struct directive {
    const char         *name;
    M_CODE            (*callback)(metha_t *, const char *);
};

#define NUM_DIRECTIVES sizeof(directives)/sizeof(struct directive)
static const struct directive directives [] = {
    {"include", &lmetha_load_config},
    {"load_module", &lmetha_load_module},
};

#define NUM_FILETYPE_OPTS sizeof(filetype_opts)/sizeof(struct lm_scope_opt)
static const struct lm_scope_opt filetype_opts[] = {
    { "extensions", LM_VAL_T_ARRAY, .value.array_set = &lm_filetype_set_extensions},
    { "mimetypes",  LM_VAL_T_ARRAY, .value.array_set = &lm_filetype_set_mimetypes},
    { "parser",     LM_VAL_T_STRING, .value.offs = offsetof(filetype_t, parser_str)},
    { "handler",    LM_VAL_T_STRING, .value.offs = offsetof(filetype_t, handler.name)},
    { "expr",       LM_VAL_T_EXTRA, .value.set = (M_CODE (*)(void *, const char *))&lm_filetype_set_expr},
    { "crawler_switch",LM_VAL_T_STRING, .value.offs = offsetof(filetype_t, switch_to.name)},
    { "attributes",LM_VAL_T_ARRAY, .value.array_set = &lm_filetype_set_attributes},
};

#define NUM_CRAWLER_OPTS sizeof(crawler_opts)/sizeof(struct lm_scope_opt)
static const struct lm_scope_opt crawler_opts[] = {
    { "filetypes",     LM_VAL_T_ARRAY,  .value.array_set = &lm_crawler_set_filetypes},
    { "dynamic_url",   LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, ftindex.dynamic_url)},
    { "extless_url",   LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, ftindex.extless_url)},
    { "unknown_url",   LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, ftindex.unknown_url)},
    { "dir_url",       LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, ftindex.dir_url)},
    { "ftp_dir_url",   LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, ftindex.ftp_dir_url)},
    { "external",      LM_VAL_T_FLAG,   .value.flag = LM_CRFLAG_EXTERNAL},
    { "external_peek", LM_VAL_T_UINT,   .value.offs = offsetof(crawler_t, peek_limit)},
    { "depth_limit",   LM_VAL_T_UINT,   .value.offs = offsetof(crawler_t, depth_limit)},
    { "initial_filetype",LM_VAL_T_STRING,.value.offs = offsetof(crawler_t, initial_filetype.name)},
    { "init",          LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, init)},
    { "spread_workers",LM_VAL_T_FLAG,   .value.flag = LM_CRFLAG_SPREAD_WORKERS},
    { "jail",          LM_VAL_T_FLAG,   .value.flag = LM_CRFLAG_JAIL},
    { "robotstxt",     LM_VAL_T_FLAG,   .value.flag = LM_CRFLAG_ROBOTSTXT},
    { "default_handler",LM_VAL_T_STRING, .value.offs = offsetof(crawler_t, default_handler.name)},
};

#define NUM_OBJECTS sizeof(objects)/sizeof(struct object)
static const struct object objects[] = {
    {
        .name         = "filetype",
        .opts         = filetype_opts,
        .num_opts     = NUM_FILETYPE_OPTS,
        .find         = &lmetha_get_filetype,
        .reset        = &lm_filetype_clear,
        .copy         = &lm_filetype_dup,
        .constructor  = (void *(*)(const char *, uint32_t))&lm_filetype_create,
        .destructor   = (void (*)(void *))0,
        .m_offs       = offsetof(metha_t, filetypes),
        .m_count_offs = offsetof(metha_t, num_filetypes),
        .flags_offs   = offsetof(filetype_t, flags),
        .size         = sizeof(filetype_t*),
    }, {
        .name         = "crawler",
        .opts         = crawler_opts,
        .num_opts     = NUM_CRAWLER_OPTS,
        .find         = &lmetha_get_crawler,
        .reset        = &lm_crawler_clear,
        .copy         = &lm_crawler_dup,
        .constructor  = (void *(*)(const char *, uint32_t))&lm_crawler_create,
        .destructor   = (void (*)(void *))0,
        .m_offs       = offsetof(metha_t, crawlers),
        .m_count_offs = offsetof(metha_t, num_crawlers),
        .flags_offs   = offsetof(filetype_t, flags),
        .size         = sizeof(crawler_t*),
    }
};

/* set maximum configuration file size to 1 MB */
#define MAX_FILESIZE 1024*1024

/**
 * This is just an ugly hack to get the line number. 
 * It will re-loop the whole string starting from 'start'
 * until it reaches 'pos' and count each line break. The 
 * speed of this function is not prioritized since it is
 * ONLY run whenever a syntax error occurs.
 **/
static inline int
lm_getline(const char *start, const char *pos)
{
    int ret = 1;
    const char *s = start;

    while (s < pos) {
        if (*s == '\n')
            ret ++;
        s ++;
    }

    return ret;
}

M_CODE
lmetha_register_scope(metha_t *m, const char *name, struct lm_scope_opt *opts, int num_opts)
{
    if (!(m->scopes = realloc(m->scopes, sizeof(lm_scope_t)*(m->num_scopes+1))))
        return M_OUT_OF_MEM;

    m->scopes[m->num_scopes].name = name;
    m->scopes[m->num_scopes].opts = opts;
    m->scopes[m->num_scopes].num_opts = num_opts;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) registered scope '%s'\n", m, name);
#endif

    m->num_scopes ++;

    return M_OK;
}

/**
 * lmetha_load_config() - Mr Pointer-Dangler
 *
 * Load a configuration file.
 *
 * TODO: If something goes wrong, there might be an object in 'o', 
 *       and if so we should call the destructor.
 **/
M_CODE
lmetha_load_config(metha_t *m, const char *filename)
{
    FILE    *fp;
    long     sz;
    char    *buf, *p;
    char    *e,   *s;
    char    *t;
    int      state    = STATE_ROOT;
    int      extend   = 0;
    int      override = 0;
    int      copy     = 0;
    int      x, y;
    int      found;
    void    *o;
    char    *path = 0;
    M_CODE   status;
    char   **array = 0;
    int      array_sz = 0;
    int      scope = 0;

    /** 
     * Note: curr will be 0 when parsing scopes (that is, NOT objects),
     * and thus lm_scope_opt->offs MUST be absolute pointers for all scope 
     * options.
     **/
    struct object       *curr;
    struct lm_scope_opt *curr_opt;
    struct directive    *curr_directive;
#ifdef DEBUG
    char *_fn = 0;
#endif

    /* TODO: don't restrict the path length to 128 chars */
    if (*filename != '/') {
        path = malloc(128);
        snprintf(path, 128, "%s/%s", m->conf_dir1, filename);
        if (!(fp = fopen(path, "r"))) {
            snprintf(path, 128, "%s/%s", m->conf_dir2, filename);
            if (!(fp = fopen(path, "r"))) {
                free(path);
                return M_COULD_NOT_OPEN;
            }
        }
#ifdef DEBUG
        _fn = path;
#endif
    } else {
        if (!(fp = fopen(filename, "r")))
            return M_COULD_NOT_OPEN;
        if (s = strrchr(filename, '/'))
            filename = s+1;
#ifdef DEBUG
        _fn = filename;
#endif
    }

    /** 
     * Make sure we havent already loaded a configuration file 
     * with this name.
     **/
    for (x=0; x<m->num_configs; x++) {
        if (strcmp(m->configs[x], filename) == 0) {
            /* this configuration file has already been loaded */
            fclose(fp);
            if (path)
                free(path);
            return M_OK;
        }
    }

    if (!(m->configs = realloc(m->configs, (m->num_configs+1)*sizeof(char*))))
        goto error;

    m->configs[m->num_configs] = strdup(filename);
    m->num_configs ++;

#ifdef DEBUG
    fprintf(stderr, "* metha:(%p) loading config '%s'\n", m, _fn);
#endif


    fseek(fp, 0, SEEK_END);

    if ((sz = ftell(fp)) > MAX_FILESIZE) {
        fclose(fp);
        return M_TOO_BIG;
    }

    fseek(fp, 0, 0);

    /* Allocate a buffer capable of holding the whole file,
     * e will point to the end of the buffer and buf and p
     * will both point to the beginning. We allocate an extra byte
     * which we will set to '\0', letting us safely use strcmp()
     * and friends. */
    e = (buf = (p = malloc(sz+1))) + sz;
    if (!p) {
        fclose(fp);
        return M_OUT_OF_MEM;
    }

    *e = '\0';

    if (!fread(p, sizeof(char), sz, fp)) {
        fclose(fp);
        if (path)
            free(path);
        return M_IO_ERROR;
    }

    fclose(fp);
    fp = 0;

    for (; p<e; p++) {
        if (isspace(*p))
            continue;
        if (*p == '#') {
            for (p++; p<e && *p != '\n'; p++)
                ;
            continue;
        }
        if (*p == '/') {
            if (p+1 < e && *(p+1) == '*') {
                /* c-style commnet */
                for (p++; p<e; p++) {
                    if (*p == '*' && p+1 < e && *(p+1) == '/') {
                        p++;
                        break;
                    }
                }
                continue;
            } else
                goto uc;
        }

        switch (state) {
            case STATE_ROOT:
                /* In the 'root' state we check for:
                 * - directives (ie, loading scripts)
                 * - crawlers
                 * - filetypes
                 **/

                for (s = p; s < e && (isalnum(*s) || *s == '_'); s++)
                    ;
                y = s-p;

                if (y == 0)
                    goto uc;

                if (y == 6 && strncmp(p, "extend", 6) == 0) {
                    p+=6;
                    if (*p != ':') {
                        LM_ERROR(m, "<%s:%d>: expected ':' after extend keyword", filename, lm_getline(buf, p));
                        goto error;
                    }

                    extend = 1;
                    continue;
                }
                if (y == 8 && strncmp(p, "override", 8) == 0) {
                    p+=8;
                    if (*p != ':') {
                        LM_ERROR(m, "<%s:%d>: expected ':' after override keyword", filename, lm_getline(buf, p));
                        goto error;
                    }

                    extend = 1;
                    override = 1;
                    continue;
                }

                found = 0;
                for (x=0; x<NUM_OBJECTS; x++) {
                    if (strncmp(p, objects[x].name, y) == 0) {
                        curr = &objects[x];
                        state = STATE_PRE_NAME;
                        p+=y-1;
                        found = 1;
                        break;
                    }
                }
                if (found)
                    continue;

                for (x=0; x<m->num_scopes; x++) {
                    if (strncmp(p, m->scopes[x].name, y) == 0) {
                        curr = &m->scopes[x];
                        scope = 1;
                        state = STATE_PRE_OBJ;
                        p+=y-1;
                        found = 1;
                        o = 0; /* by setting o to 0, scopes can have absolute pointers
                                  as their "member offset" and the code below will still run
                                  properly */
                        break;
                    }
                }
                if (found)
                    continue;

                for (x=0; x<NUM_DIRECTIVES; x++) {
                    if (strncmp(p, directives[x].name, y) == 0) {
                        p+=y;
                        found = 1;
                        state = STATE_DIRECTIVE_ARG;
                        curr_directive = &directives[x];
                        break;
                    }
                }

                if (found)
                    continue;

                goto uc;

            case STATE_DIRECTIVE_ARG:
                /* reach here when we need to parse out an argument for a given directive */
                if (*p == '"') {
                    p++;
                    t = p+strcspn(p, "\"\n");
                    if (!*t || *t == '\n') {
                        LM_ERROR(m, "<%s:%d>: unterminated string constant", filename, lm_getline(buf, p));
                        goto error;
                    }

                    *t = '\0';

                    if ((status = curr_directive->callback(m, p)) != M_OK) {
                        LM_ERROR(m, "<%s:%d>: %s \"%s\" failed: %s\n", filename, lm_getline(buf, p), curr_directive->name, p, lm_strerror(status));
                        goto error;
                    }

                    p = t;
                } else {
                    LM_ERROR(m, "<%s:%d>: expected a quoted argument for directive '%s', found '%c'\n", filename, lm_getline(buf, p), curr_directive->name, *p);
                    goto error;
                }
                state = STATE_ROOT;
                break;

            case STATE_PRE_NAME:
                if (*p != '[') {
                    LM_ERROR(m, "<%s:%d>: expected '[', found '%c'", filename, lm_getline(buf, p), *p);
                    goto error;
                }
                state = STATE_NAME;
                break;

            case STATE_POST_NAME:
                if (strncmp(p, "copy", 4) == 0) {
                    copy = 1;
                    p+=3;

                    state = STATE_NAME;
                    continue;
                }

            case STATE_END_NAME:
                if (*p == ']') {
                    state = STATE_PRE_OBJ;
                    continue;
                }
                goto uc;
                
            case STATE_NAME:
                if (*p == '"') {
                    p++;
                    t = p+strcspn(p, "\"\n");
                    if (!*t || *t == '\n') {
                        LM_ERROR(m, "<%s:%d>: unterminated string constant", filename, lm_getline(buf, p));
                        goto error;
                    }

                    *t = '\0';

                    if (!(t-p)) {
                        LM_ERROR(m, "<%s:%d>: empty %s name", filename, lm_getline(buf, p), curr->name);
                        goto error;
                    }

                    if (!copy) {
                        /* Now that we have a name for the object, we can call the 
                         * constructor. The constructor must expect two arguments,
                         * a pointer to the name of the object and its length. */
                        if (!extend)
                            o = curr->constructor(p, (uint32_t)(t-p));
                        else {
                             /* if 'extend' is set to 1, then we are extending an already 
                              * defined object, so we must look it up and modify it. */
                            if (!(o = curr->find(m, p))) {
                                LM_ERROR(m, "<%s:%d>: undefined %s '%s'", filename, lm_getline(buf, p), curr->name, p);
                                goto error;
                            }
                            /* override might also be set, and if so then we should
                             * call the object's reset function */
                            if (override)
                                curr->reset(o);

                        }
                        state = STATE_POST_NAME;
                    } else {
                        /* reach here if we want the name for an object
                         * after the 'copy' keyword */
                        void *cp;
                        if (!(cp = curr->find(m, p))) {
                            LM_ERROR(m, "<%s:%d>: undefined %s '%s'", filename, lm_getline(buf, p), curr->name, p);
                            goto error;
                        }
#ifdef DEBUG
                        fprintf(stderr, "* metha:(%p) copy %s '%s' to '%s'\n", m, curr->name, p, ((filetype_t*)o)->name);
#endif
                        if (curr->copy(o, cp) != M_OK) {
                            LM_ERROR(m, "<%s:%d>: internal error while copying %s '%s' to '%s'", filename, lm_getline(buf, p), curr->name, p, ((filetype_t*)o)->name);
                            goto error;
                        }

                        copy = 0;
                        state = STATE_END_NAME;
                    }

                    p = t;
                } else {
                    LM_ERROR(m, "<%s:%d>: expected quoted %s name, found '%c'", filename, lm_getline(buf, p), curr->name, *p);
                    goto error;
                }
                continue;

            case STATE_PRE_OBJ:
                if (*p == '{') {
                    state = STATE_OBJ;
                    continue;
                } else if (*p == ';') {
                    state = STATE_ROOT;
                    continue;
                }
                LM_ERROR(m, "<%s:%d>: expected '{' or ';', found '%c'", filename, lm_getline(buf, p), *p);
                goto error;

            case STATE_OBJ:
                if (*p == '}') {
                    if (!scope) {
                        /* We reach here once we are done parsing this "object". So now 
                         * we will add the object to the actual metha object. */

                        /* if we extended an already defined object, there's nothing 
                         * more to do here since the object is already added */
                        if (!extend) {
                            /* TODO: use functions instead, lmetha_add_filetype() etc */

                            /* first we check whether there are objects of the same type 
                             * already added to the specific object array, and if so we 
                             * will prefer realloc(), elsewise we simply do a malloc 
                             * on the object size */
                            const struct object *ct = curr;
                            int    *count = (int*)(((char *)m)+ct->m_count_offs);
                            void ***array = (void ***)(((char *)m)+ct->m_offs);
                            if (*count)
                                (*array) = realloc(*array, ((*count)+1)*ct->size);
                            else
                                (*array) = malloc(ct->size);
                            ((*array)[*count]) = o;
                            (*count) ++;
                        } else {
                            extend = 0;
                            override = 0;
                        }
                    } else {
                        /* we are done parsing a scope, so there's not much to do
                         * other than setting scope to 0 and continuing */
                        scope = 0;
                    }

                    o = 0;
                    state = STATE_ROOT;
                    continue;
                }

                x = 1;
                while (isalnum(*(p+x)) || *(p+x) == '-' || *(p+x) == '_')
                    x++;

                found = 0;
                for (y=0; y<curr->num_opts; y++) {
                    if (strncmp(p, curr->opts[y].name, x) == 0 && curr->opts[y].name[x] == '\0') {
                        found = 1;
                        p += x-1;
                        curr_opt = &curr->opts[y];
                        break;
                    }
                }

                if (!found) {
                    *(p+x) = '\0';
                    if (isalnum(*p)) {
                        LM_ERROR(m, "<%s:%d>: unknown option '%s'", filename, lm_getline(buf, p), p);
                        goto error;
                    }
                    goto uc;
                }

                state = STATE_PRE_VAL;
                continue;

            case STATE_PRE_VAL:
                if (*p != '=') {
                    LM_ERROR(m, "<%s:%d>: expected '=', found '%c'", filename, lm_getline(buf, p), *p);
                    goto error;
                }
                state = STATE_VAL;
                continue;

            case STATE_VAL:
                /* This is where we parse the actual value for each option, 
                 * some options expect strings, others integers, and some expect 
                 * arrays. */
                if (*p == '{') {
                    if (scope) {
                        LM_ERROR(m, "<%s:%d>: arrays not supported by scopes", filename, lm_getline(buf, p));
                        goto error;
                    } else if (curr_opt->type == LM_VAL_T_ARRAY) {
                        state = STATE_ARRAY_VAL;
                        continue;
                    } else {
                        LM_ERROR(m, "<%s:%d>: option '%s' expects a value of type %s", filename, lm_getline(buf, p), curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                } else if (*p == '"') {
                    if (curr_opt->type == LM_VAL_T_STRING
                            || curr_opt->type == LM_VAL_T_EXTRA) {
                        p++;

                        t = p+strcspn(p, "\"\n");
                        if (!*t || *t == '\n') {
                            LM_ERROR(m, "<%s:%d>: unterminated string constant", filename, lm_getline(buf, p));
                            goto error;
                        }

                        *t = '\0';
                        if (curr_opt->type == LM_VAL_T_EXTRA) {
                            curr_opt->value.set(o, p);
                        } else
                            *(char**)((char *)o+curr_opt->value.offs) = strdup(p);

                        p = t;
                    } else {
                        LM_ERROR(m, "<%s:%d>: option '%s' expects a value of type %s", filename, lm_getline(buf, p), curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                } else if (isdigit(*p)) {
                    if (curr_opt->type == LM_VAL_T_UINT) {
                        *(unsigned int*)((char *)o+curr_opt->value.offs) = atoi(p);
                    } else if (curr_opt->type == LM_VAL_T_FLAG) {
                        if (scope) {
                            LM_ERROR(m, "<%s:%d>: flags not supported by scopes", filename, lm_getline(buf, p));
                            goto error;
                        }
                        if (atoi(p))
                            *(uint8_t*)((char *)o+curr->flags_offs) |= (1 << curr_opt->value.flag);
                    } else {
                        LM_ERROR(m, "<%s:%d>: option '%s' expects a value of type %s", filename, lm_getline(buf, p), curr_opt->name, val_str[curr_opt->type]);
                        goto error;
                    }
                    do p++; while (isdigit(*p));
                } else {
                    if (curr_opt->type == LM_VAL_T_FLAG) {
                        if (scope) {
                            LM_ERROR(m, "<%s:%d>: flags not supported by scopes", filename, lm_getline(buf, p));
                            goto error;
                        }
                        if (strncasecmp(p, "true", 4) == 0) {
                            *(uint8_t*)((char *)o+curr->flags_offs) |= (1 << curr_opt->value.flag);
                            p+=4;
                        } else if (strncasecmp(p, "false", 5) == 0) {
                            p+=5;
                        } else {
                            LM_ERROR(m, "<%s:%d>: expected %s, found '%c'", filename, lm_getline(buf, p), val_str[curr_opt->type], *p);
                            goto error;
                        }
                    } else {
                        LM_ERROR(m, "<%s:%d>: expected %s, found '%c'", filename, lm_getline(buf, p), val_str[curr_opt->type], *p);
                        goto error;
                    }
                }
                state = STATE_POST_VAL;
                break;

            case STATE_POST_VAL:
                if (*p != ';') {
                    LM_ERROR(m, "<%s:%d>: expected ';', found '%c'", filename, lm_getline(buf, p), *p);
                    goto error;
                }
                state = STATE_OBJ;
                continue;

            case STATE_ARRAY_PRE_VAL:
                /* state between all values in an array */
                if (*p == '}') {
                    state = STATE_POST_VAL;

                    /* we are done parsing the array, time to send the 
                     * built array to the callback function for this
                     * variable */
                    curr_opt->value.array_set(o, array, array_sz);
                    array = 0;
                    array_sz = 0;
                }
                else if (*p == ',')
                    state = STATE_ARRAY_VAL;
                else
                    goto uc;
                continue;

            case STATE_ARRAY_VAL:
                if (*p != '"')
                    goto uc;

                p++;
                t = p+strcspn(p, "\"\n");
                if (!*t || *t == '\n') {
                    LM_ERROR(m, "<%s:%d>: unterminated string constant", filename, lm_getline(buf, p));
                    goto error;
                }

                *t = '\0';

                /* current value in the array will start at p and reach to t */

                if (!(array = realloc(array, (array_sz+1)*sizeof(char*))))
                    return M_OUT_OF_MEM;
                if (!(array[array_sz] = malloc((t-p)+1)))
                    return M_OUT_OF_MEM;

                memcpy(array[array_sz], p, (t-p)+1);

                array_sz ++;

                p = t;
                state = STATE_ARRAY_PRE_VAL;
                continue;

            default:
                goto error;
        }
    }

    if (state != STATE_ROOT) {
        LM_ERROR(m, "<%s>: unexpected end of file", filename);
        goto error;
    }

    if (array) free(array);
    if (buf) free(buf);
    if (path) free(path);
    return M_OK;

uc:
    LM_ERROR(m, "<%s:%d>: unexpected char '%c'", filename, lm_getline(buf, p), *p);

error:
    if (array) free(array);
    if (fp) fclose(fp);
    if (buf) free(buf);
    if (path) free(path);
    return M_FAILED;
}

