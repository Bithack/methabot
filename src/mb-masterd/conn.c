#include "master.h"

#include <syslog.h>
#include <pthread.h>
#include <string.h>

enum {
    MBM_AUTH_TYPE_CLIENT,
    MBM_AUTH_TYPE_SLAVE,
    MBM_AUTH_TYPE_STATUS,
    MBM_AUTH_TYPE_OPERATOR,
};

#define NUM_AUTH_TYPES sizeof(auth_types)/sizeof(char*)
const char *auth_types[] = {
    "client",
    "slave",
    "status",
    "operator",
};

void *
mbm_conn_main(void* in)
{
    int sock = (int)in;
    char buf[256];
    int sz;
    int authenticated = 0;
    char *user;
    char *pwd;
    char *type;
    int auth;
    int x;
    int auth_tries = 0;

    /**
     * TODO: possible 301 Temporarily Unavailable message
     **/
    send(sock, "100 Ready\n", 10, MSG_NOSIGNAL);

    while ((sz = recv(sock, buf, 256, MSG_NOSIGNAL))) {
        if (sz == -1)
            break; /* connection broken */

        buf[sz] = '\0';
        char *p = buf;

        if (authenticated) {
            switch (auth) {
                case MBM_AUTH_TYPE_SLAVE:
                case MBM_AUTH_TYPE_STATUS:
                case MBM_AUTH_TYPE_OPERATOR:
                    break;
                default:
                    send(sock, "300 Internal Error\n", 19, MSG_NOSIGNAL);
                    goto close;
            }
        } else {
            if (memcmp(buf, "AUTH", 4) == 0) {
                p += 4;

                while (*p == ' ') {
                    if (!*p)
                        goto invalid;
                    p++;
                }
                type = p;
                while (*p != ',') {
                    if (!*p)
                        goto invalid;
                    p++;
                }
                *p = '\0';
                p++;
                user = p;
                while (*p != ',') {
                    if (!*p)
                        goto invalid;
                    p++;
                }
                *p = '\0';
                pwd = p+1;

                /* verify the auth type */
                int auth_found = 0;
                for (x=0; x<NUM_AUTH_TYPES; x++) {
                    if (strcmp(type, auth_types[x]) == 0) {
                        auth = x;
                        auth_found = 1;
                        break;
                    }
                }
                
                if (auth_found) {
                    /* first argument is the type, this can be client, slave, status or operator */
                    syslog(LOG_INFO, "AUTH type=%s,user=%s OK from #%d", type, user, sock);
                    authenticated = 1;

                    switch (auth) {
                        case MBM_AUTH_TYPE_CLIENT:
                            send(sock, "101 OK\n", 7, MSG_NOSIGNAL);
                            send(sock, "SLAVE a792bf5f-a23b-1,127.0.0.1:5001\n", strlen("SLAVE a792bf5f-a23b-1,127.0.0.1:5001\n"), MSG_NOSIGNAL);
                            break;

                        case MBM_AUTH_TYPE_SLAVE:
                            send(sock, "100 OK\n", 7, MSG_NOSIGNAL);
                            send(sock, "CLIENT a792bf5f-a23b-1,127.0.0.1:5001\n", strlen("CLIENT a792bf5f-a23b-1,127.0.0.1:5001\n"), MSG_NOSIGNAL);
                            break;

                        default:
                            goto close;
                    }

                    continue;
                }

                send(sock, "200 DENIED\n", 11, MSG_NOSIGNAL);
                auth_tries++;
                if (auth_tries >= 3)
                    goto close;
            } else
                goto invalid;
        }
    }

close:
    close(sock);
    return 0;

invalid:
    syslog(LOG_WARNING, "invalid data from #%d, closing connection", sock);
    close(sock);
    return 0;
}
