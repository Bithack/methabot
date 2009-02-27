#ifndef _M_SLAVE__H_
#define _M_SLAVE__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "conn.h"

#define MBS_DEFAULT_PORT 5305

struct slave {
    struct sockaddr_in addr;
    struct sockaddr_in master;

    char *config_buf;
    int   config_sz;

    char *user;
    char *pass;
    int   master_sock;

    ev_io     master_io;
    /* timer for reconnecting to the master if the connection is lost */
    ev_timer  master_timer;
};

extern struct slave srv;

#endif
