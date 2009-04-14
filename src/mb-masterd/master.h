#ifndef _MASTER__H_
#define _MASTER__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "conn.h"
#include "mysql.h"

#define MBM_DEFAULT_PORT 5505

struct crawler;
struct filetype;

struct master {
    MYSQL *mysql;
    struct sockaddr_in addr;
    char *config_file;
    char *config_buf;
    int   config_sz;

    char *user;
    char *group;

    struct conn **pool;
    unsigned num_conns;

    struct slave *slaves;
    unsigned num_slaves;

    struct crawler **crawlers;
    unsigned         num_crawlers;

    struct filetype **filetypes;
    unsigned         num_filetypes;

    struct {
        struct {
            char *buf;
            int   sz;
        } slave_list;
    } xml;

    struct {
        const char *session_complete;
    } hooks;
};

extern struct master srv;

void *mbm_conn_main(void *in);
void strrmsq(char *s);

#endif
