#ifndef _MASTER__H_
#define _MASTER__H_

#include <netinet/in.h>
#include <arpa/inet.h>

#define MBM_DEFAULT_PORT 5304

struct master {
    struct sockaddr_in addr;
    char *config_file;
    char *config_buf;
    int   config_sz;
};

extern struct master srv;

void *mbm_conn_main(void *in);

#endif
