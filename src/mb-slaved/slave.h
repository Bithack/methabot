#ifndef _M_SLAVE__H_
#define _M_SLAVE__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../libmetha/libev/ev.h"

#define MBS_DEFAULT_PORT 5305
#define MAX_NUM_PENDING  128

enum {
    SLAVE_MSTATE_COMMAND,
    SLAVE_MSTATE_RECV_CONF,
};

struct slave {
    int mstate;

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

    struct client  **pending;
    int              num_pending;
    pthread_mutex_t  pending_lk;
    struct client **clients;
    int             num_clients;
};

extern struct slave srv;

int sock_getline(int fd, char *buf, int max);

#endif
