/*-
 * main.c
 * This file is part of mb-masterd
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "master.h"
#include "conn.h"
#include "conf.h"
#include "../libmetha/libev/ev.c"

int mbm_load_config();
int mbm_main();
int mbm_cleanup();
static void mbm_ev_sigint(EV_P_ ev_signal *w, int revents);

static const char *_cfg_file;
struct master srv;

static M_CODE set_config_cb(void *unused, const char *val);
static M_CODE set_listen_cb(void *unused, const char *val);
static M_CODE set_session_complete_hook_cb(void *unused, const char *val);

#define NUM_OPTS (sizeof(opts)/sizeof(struct lmc_directive))
static struct lmc_directive opts[] = {
    {"listen", set_listen_cb},
    {"config", set_config_cb},
    {"session_complete_hook", set_session_complete_hook_cb},
};

int
main(int argc, char **argv)
{
    pid_t pid, sid;
    FILE *fp = 0;

    /*

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);
    sid = setsid();

    if (sid < 0)
        exit(EXIT_FAILURE);
        */

    if (argc > 1)
        _cfg_file = argv[1];
    else
        _cfg_file = "/etc/mb-masterd.conf";

    signal(SIGPIPE, SIG_IGN);
    openlog("mb-masterd", 0, 0);
    syslog(LOG_INFO, "started");

    /*close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);*/

    if (mbm_load_config() != 0) {
        syslog(LOG_ERR, "loading configuration file failed");
        goto error;
    }

    if (srv.config_file) {
        /* load the configuration file */
        if (!(fp = fopen(srv.config_file, "rb"))) {
            syslog(LOG_ERR, "could not open file '%s'", srv.config_file);
            goto error;
        }

        /* calculate the file length */
        fseek(fp, 0, SEEK_END);
        srv.config_sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (!(srv.config_buf = malloc(srv.config_sz))) {
            syslog(LOG_ERR, "out of mem");
            goto error;
        }

        if ((size_t)srv.config_sz != fread(srv.config_buf, 1, srv.config_sz, fp)) {
            syslog(LOG_ERR, "read error");
            goto error;
        }

        fclose(fp);
        fp = 0;

        if (mbm_mysql_connect() != 0) {
            syslog(LOG_ERR, "could not connect to mysql server");
            goto error;
        }
        if (mbm_mysql_setup() != 0) {
            syslog(LOG_ERR, "setting up mysql tables failed");
            goto error;
        }
        if (mbm_reconfigure() != 0) {
            syslog(LOG_ERR, "setting up filetype tables failed");
            goto error;
        }

        mbm_start();
    } else {
        syslog(LOG_ERR, "no configuration file, exiting");
        goto error;
    }
    /*
    if (mkfifo("/var/run/mb-masterd/mb-masterd.sock", 770) != 0) {
        fprintf("error: unable to create mb-masterd.sock\n");
        return 1;
    }
    */

    mbm_cleanup();
    closelog();
    return 0;

error:
    mbm_cleanup();
    if (fp)
        fclose(fp);
    closelog();
    return 1;
}

int
mbm_mysql_connect()
{
    if (!(srv.mysql = mysql_init(0))
            || !(mysql_real_connect(srv.mysql, "localhost",
                            "methanol", "test", "methanol",
                            0, "/var/run/mysqld/mysqld.sock", 0)))
        return -1;

    return 0;
}

#define SQL_USER_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_user (id INT NOT NULL AUTO_INCREMENT, user VARCHAR(32), pass VARCHAR(32), PRIMARY KEY (id))"
#define SQL_CLIENT_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_client ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    token VARCHAR(40), \
                    PRIMARY KEY (id) \
                    )"
#define SQL_SLAVE_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_slave ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    user VARCHAR(64), \
                    PRIMARY KEY (id) \
                    )"
#define SQL_URL_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_url ( \
                    hash VARCHAR(40), \
                    url VARCHAR(4096), \
                    date DATETIME, \
                    PRIMARY KEY (hash) \
                    )"
#define SQL_ADDED_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_added ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    user_id INT, \
                    crawler VARCHAR(64), \
                    input VARCHAR(4096), \
                    date DATETIME, \
                    PRIMARY KEY (id),\
                    INDEX (user_id)\
                    )"
#define SQL_MSG_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_msg ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `to` INT, \
                    `from` INT, \
                    `date` DATETIME, \
                    title VARCHAR(255), \
                    content TEXT, \
                    PRIMARY KEY (id), \
                    INDEX (`to`) \
                    )"
#define SQL_SESS_TBL "\
            CREATE TABLE IF NOT EXISTS \
            nol_session ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `added_id` INT, \
                    `client_id` INT, \
                    `date` DATETIME, \
                    `latest` DATETIME, \
                    `state` ENUM('running','wait-hook','hook','done') DEFAULT 'running', \
                    `report` TEXT, \
                    PRIMARY KEY (id)\
                    )"
#define SQL_SESS_REL_UNIQ "\
            ALTER TABLE nol_session_rel ADD UNIQUE (\
                `session_id`, \
                `target_id` \
            );"
#define SQL_SESS_REL_TBL "\
            CREATE TABLE \
            nol_session_rel ( \
                    session_id INT NOT NULL, \
                    filetype VARCHAR(64), \
                    `target_id` INT NOT NULL, \
                    INDEX (session_id)\
                    )"

/*
#define SQL_CRAWLERS_DROP "DROP TABLE IF EXISTS nol_crawlers;"
#define SQL_CRAWLERS_TBL "\
            CREATE TABLE \
            nol_crawlers ( \
                    id INT NOT NULL AUTO_INCREMENT, \
                    `to` INT, \
                    `from` INT, \
                    `date` DATETIME, \
                    title VARCHAR(255), \
                    content TEXT, \
                    PRIMARY KEY (id), \
                    INDEX (`to`) \
                    )"
                    */
#define SQL_USER_CHECK "SELECT null FROM nol_user LIMIT 0,1;"
#define SQL_ADD_DEFAULT_USER \
    "INSERT INTO nol_user (user, pass) VALUES ('admin', 'admin')"
/** 
 * Set up all default tables
 **/
int
mbm_mysql_setup()
{
    MYSQL_RES *r;
    long       id;
    char msg[512];
    mysql_real_query(srv.mysql, SQL_USER_TBL, sizeof(SQL_USER_TBL)-1);
    mysql_real_query(srv.mysql, SQL_CLIENT_TBL, sizeof(SQL_CLIENT_TBL)-1);
    mysql_real_query(srv.mysql, SQL_SLAVE_TBL, sizeof(SQL_SLAVE_TBL)-1);
    mysql_real_query(srv.mysql, SQL_URL_TBL, sizeof(SQL_URL_TBL)-1);
    mysql_real_query(srv.mysql, SQL_ADDED_TBL, sizeof(SQL_ADDED_TBL)-1);
    mysql_real_query(srv.mysql, SQL_MSG_TBL, sizeof(SQL_MSG_TBL)-1);
    mysql_real_query(srv.mysql, SQL_SESS_TBL, sizeof(SQL_SESS_TBL)-1);
    if (mysql_real_query(srv.mysql, SQL_SESS_REL_TBL,
            sizeof(SQL_SESS_REL_TBL)-1) == 0) {
        mysql_real_query(srv.mysql, SQL_SESS_REL_UNIQ,
                sizeof(SQL_SESS_REL_UNIQ)-1);
    }

    /* if there's no user added to the user table, then we
     * must add the default one with admin:admin as login */
    if (mysql_real_query(srv.mysql, SQL_USER_CHECK, sizeof(SQL_USER_CHECK)-1) != 0)
        return -1;
    if (!(r = mysql_store_result(srv.mysql)))
        return -1;
    if (!mysql_num_rows(r)) {
        mysql_real_query(srv.mysql, SQL_ADD_DEFAULT_USER,
                sizeof(SQL_ADD_DEFAULT_USER)-1);
        id = mysql_insert_id(srv.mysql);
        /* also send a message to the admin user telling him to
         * change his password as soon as possible */
        int  len;
        len = sprintf(msg,
                "INSERT INTO nol_msg (`to`, `from`, `date`, `title`, `content`)"
                "VALUES (%d, NULL, NOW(), 'Password must be changed!', "
                "'You are currently logged in with the default "
                "account created on system startup. It is strongly "
                "recommended that you change your password.')",
                           (int)id);
        mysql_real_query(srv.mysql, msg, len);
    }
    mysql_free_result(r);

    return 0;
}

#define CREATE_TBL         "CREATE TABLE IF NOT EXISTS "
#define CREATE_TBL_LEN     (sizeof(CREATE_TBL)-1)
#define DEFAULT_LAYOUT     \
    "(id INT NOT NULL AUTO_INCREMENT, "\
    "url_hash VARCHAR(40), "\
    "date DATETIME, "\
    "PRIMARY KEY (id),"\
    "UNIQUE (url_hash))"
#define DEFAULT_LAYOUT_LEN (sizeof(DEFAULT_LAYOUT)-1)

enum {
    ATTR_TYPE_UNKNOWN,
    ATTR_TYPE_INT,
    ATTR_TYPE_TEXT,
    ATTR_TYPE_BLOB,
};

/** 
 * mbm_reconfigure()
 * Create filetype tables.
 *
 * When a filetype is created, it will contain only one column
 * at first, and then ALTER TABLE is used to add the filetype's
 * attributes one by one. The benefit of adding the columns one,
 * by one is that whenever a filetype has been changed in the
 * configuration file, its new attributes will be added as new 
 * columns, while the existing columns will be untouched, thus
 * the system can easily reconfigure itself while retaining
 * already existing columns and data.
 *
 * No columns will be removed from filetype tables, since 
 * they might contain important data. It is up to the system
 * admin to remove unused columns.
 **/
int
mbm_reconfigure()
{
    int  x, a;
    int  n;
    char tq[64+64+CREATE_TBL_LEN+DEFAULT_LAYOUT_LEN+1];
    char *name;
    int  len;
    lmc_parser_t *lmc;

    /* the lmc parser is created with 0 as root, since
     * our object-functions will modify the static 'srv'
     * directly */
    if (!(lmc = lmc_create(0)))
        return -1;

    lmc_add_class(lmc, &mbm_filetype_class);
    lmc_add_class(lmc, &mbm_crawler_class);
    lmc_parse_file(lmc, srv.config_file);

    for (n=0; n<srv.num_filetypes; n++) {
        name = srv.filetypes[n]->name;
        len = strlen(name);

        /** 
         * Make sure the name does not contain any weird
         * characters. Convert them to '_'.
         **/
        for (x=0; x<len; x++)
            if (!isalnum(*(name+x)) && *(name+x) != '_')
                *(name+x) = '_';

        len = sprintf(tq, "%sft_%.60s%s", 
                            CREATE_TBL,
                            name,
                            DEFAULT_LAYOUT
                );

        mysql_real_query(srv.mysql, tq, len);

        /** 
         * Now add all the columns. Many of these queries will probably
         * fail because the column already exists, but this is expected
         * behaviour.
         **/
        for (a=0; a<srv.filetypes[n]->num_attributes; a++) {
            char *attr = srv.filetypes[n]->attributes[a];
            char *type = strchr(attr, ' ');
            int type_n;

            if (!type || !strlen(type+1)) {
                syslog(LOG_ERR, "no type given for attribute '%s'", attr);
                continue;
            }
            type++;
            type_n = ATTR_TYPE_UNKNOWN;
            if (strcasecmp(type, "INT") == 0)
                type_n = ATTR_TYPE_INT;
            else if (strcasecmp(type, "TEXT") == 0)
                type_n = ATTR_TYPE_TEXT;
            else if (strcasecmp(type, "BLOB") == 0)
                type_n = ATTR_TYPE_BLOB;

            if (type_n == ATTR_TYPE_UNKNOWN) {
                syslog(LOG_ERR, "unknown attribute type '%s'", type);
                continue;
            }

            len = sprintf(tq, "ALTER TABLE ft_%.60s ADD COLUMN %.60s",
                    name, attr);
            if (mysql_real_query(srv.mysql, tq, len) == 0) {
                if (type_n == ATTR_TYPE_TEXT) {
                    *(type-1) = '\0';
                    sprintf(tq, "ALTER TABLE ft_%.60s ADD FULLTEXT (%.60s)",
                            name, attr);
                    mysql_real_query(srv.mysql, tq, len);
                }
            }

        }
    }

    lmc_destroy(lmc);

    return 0;
}

int mbm_start()
{
    struct ev_loop *loop;
    int sock;

    ev_io     io_listen;
    ev_signal sigint_listen;
    
    if (!(loop = ev_default_loop(EVFLAG_AUTO)))
        return 1;

    srv.addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    int o = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o));
    if (bind(sock, (struct sockaddr*)&srv.addr, sizeof srv.addr) == -1) {
        syslog(LOG_ERR, "could not bind to %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    syslog(LOG_INFO, "listening on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));

    ev_signal_init(&sigint_listen, &mbm_ev_sigint, SIGINT);
    ev_io_init(&io_listen, &mbm_ev_conn_accept, sock, EV_READ);

    /* catch SIGINT */
    ev_signal_start(loop, &sigint_listen);
    ev_io_start(loop, &io_listen);

    ev_loop(loop, 0);

    close(sock);

    ev_default_destroy();

    return 0;
}

/** 
 * Called when sigint is recevied
 **/
static void
mbm_ev_sigint(EV_P_ ev_signal *w, int revents)
{
    int x;

    ev_signal_stop(EV_A_ w);

    syslog(LOG_INFO, "SIGINT received\n");

    if (srv.num_conns) {
        for (x=0; x<srv.num_conns; x++) {
            close(srv.pool[x]->sock); 
            free(srv.pool[x]);
        }

        free(srv.pool);
    }

    ev_unloop(EV_A_ EVUNLOOP_ALL);
}

int
mbm_cleanup()
{
    int x;

    if (srv.num_filetypes) {
        for (x=0; x<srv.num_filetypes; x++)
            mbm_filetype_destroy(srv.filetypes[x]);
        free(srv.filetypes);
    }

    if (srv.num_crawlers) {
        for (x=0; x<srv.num_crawlers; x++)
            mbm_crawler_destroy(srv.crawlers[x]);
        free(srv.crawlers);
    }

    if (srv.config_file)
        free(srv.config_file);
    if (srv.config_sz)
        free(srv.config_buf);
}

int
mbm_load_config()
{
    FILE *fp;
    char in[256];
    char *p;
    char *s;
    int len;

    int x;
    lmc_parser_t *lmc;

    if (!(lmc = lmc_create(&srv)))
        return 0;

    for (x=0; x<NUM_OPTS; x++)
        lmc_add_directive(lmc, &opts[x]);

    syslog(LOG_INFO, "loading '%s'", _cfg_file);

    if (lmc_parse_file(lmc, _cfg_file) != M_OK) {
        syslog(LOG_ERR, "parsing config failed: %s", lmc->last_error?lmc->last_error:"");
        lmc_destroy(lmc);
        return 1;
    }

    lmc_destroy(lmc);
    return 0;
}

static M_CODE
set_listen_cb(void *unused, const char *val)
{
    char *s;
    if ((s = strchr(val, ':'))) {
        srv.addr.sin_port = htons(atoi(s+1));
        *s = '\0';
    } else
        srv.addr.sin_port = htons(MBM_DEFAULT_PORT);
    srv.addr.sin_addr.s_addr = inet_addr(val);

    return M_OK;
}


static M_CODE
set_config_cb(void *unused, const char *val)
{
    if (!(srv.config_file = strdup(val)))
        return M_OUT_OF_MEM;
    return M_OK;
}

static M_CODE
set_session_complete_hook_cb(void *unused, const char *val)
{
    if (!(srv.hooks.session_complete = strdup(val)))
        return M_OUT_OF_MEM;
    return M_OK;
}

