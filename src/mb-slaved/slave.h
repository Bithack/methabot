#ifndef _M_SLAVE__H_
#define _M_SLAVE__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "../libmetha/libev/ev.h"

#define MBS_DEFAULT_PORT 5305

enum {
    SLAVE_STATE_IDLE,
    SLAVE_STATE_RECV_CONF,
};

struct slave {
    int state;

    struct sockaddr_in addr;
    struct sockaddr_in master;

    char *config_buf;
    unsigned int   config_sz;
    unsigned int   config_cap;

    char *user;
    char *pass;
    int   master_sock;

    ev_io     master_io;
    /* timer for reconnecting to the master if the connection is lost */
    ev_timer  master_timer;
};

extern struct slave srv;

#endif
