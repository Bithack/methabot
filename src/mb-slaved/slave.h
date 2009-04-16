#ifndef _M_SLAVE__H_
#define _M_SLAVE__H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mysql/mysql.h>

#include "../libmetha/libev/ev.h"

#define NOL_SLAVE_DEFAULT_PORT 5506
#define MAX_NUM_PENDING  128

enum {
    SLAVE_MSTATE_COMMAND,
    SLAVE_MSTATE_RECV_CONF,
};

struct opt_vals {
    char *listen;
    char *master;
    char *user;
    char *group;
    char *mysql_host;
    unsigned int mysql_port;
    char *mysql_sock;
    char *mysql_user;
    char *mysql_pass;
    char *mysql_db;
} opt_vals;

struct slave {
    int                 mstate;
    MYSQL              *mysql;

    int                 listen_sock;
    struct sockaddr_in  addr;
    struct sockaddr_in  master;

    char *config_buf;
    unsigned int   config_sz;
    unsigned int   config_cap;

    char *user;
    char *pass;
    int   master_sock;

    ev_io     master_io;
    /* timer for reconnecting to the master if the connection is lost */
    ev_timer  master_timer;
    ev_async  client_status;

    struct client  **pending;
    int              num_pending;
    pthread_mutex_t  pending_lk;
    struct client **clients;
    int             num_clients;
    pthread_mutex_t  clients_lk;
};

extern struct slave srv;
extern struct opt_vals opt_vals;

int sock_getline(int fd, char *buf, int max);
MYSQL *mbs_dup_mysql_conn(void);

#endif
