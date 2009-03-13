#ifndef _MASTER__H_
#define _MASTER__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "conn.h"
#include "mysql.h"

#define MBM_DEFAULT_PORT 5304

struct crawler;
struct filetype;

struct master {
    MYSQL *mysql;
    struct sockaddr_in addr;
    char *config_file;
    char *config_buf;
    int   config_sz;

    struct conn **pool;
    unsigned num_conns;

    struct slave *slaves;
    unsigned num_slaves;

    struct crawler **crawlers;
    unsigned         num_crawlers;

    struct filetype **filetypes;
    unsigned         num_filetypes;
};

extern struct master srv;

void *mbm_conn_main(void *in);

#endif
