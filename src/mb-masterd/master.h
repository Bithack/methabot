#ifndef _MASTER__H_
#define _MASTER__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "conn.h"
#include "mysql.h"

#define NOL_MASTER_DEFAULT_PORT 5505

struct crawler;
struct filetype;

struct opt_val_list {
    char *listen;
    char *config_file;
    char *user;
    char *group;
    char *session_complete_hook;
    char *mysql_host;
    unsigned int mysql_port;
    char *mysql_sock;
    char *mysql_user;
    char *mysql_pass;
    char *mysql_db;
};

/* run-time generated information */
struct master {
    MYSQL             *mysql;
    struct sockaddr_in addr;
    int                listen_sock;
    char              *config_buf;
    int                config_sz;

    struct conn      **pool;
    unsigned           num_conns;
    struct slave      *slaves;
    unsigned           num_slaves;
    struct filetype  **filetypes;
    unsigned           num_filetypes;
    struct crawler   **crawlers;
    unsigned           num_crawlers;

    struct {
        struct {
            char *buf;
            int   sz;
        } slave_list;
    } xml;
};

extern struct master srv;
extern struct opt_val_list opt_vals;

void *mbm_conn_main(void *in);
void strrmsq(char *s);

#endif
