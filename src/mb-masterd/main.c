#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "master.h"
#include "conn.h"
#include "../libmetha/libev/ev.c"

int mbm_load_config();
int mbm_main();
int mbm_cleanup();
static void mbm_ev_sigint(EV_P_ ev_signal *w, int revents);

struct master srv;

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

    openlog("mb-masterd", 0, 0);
    syslog(LOG_INFO, "started");

    close(stdin);
    close(stdout);
    close(stderr);

    if (mbm_load_config() != 0)
        goto error;

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
    } else
        srv.config_sz = 0;

    mbm_start();

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
    if (fp)
        fclose(fp);
    closelog();
    return 1;
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

    if (bind(sock, (struct sockaddr*)&srv.addr, sizeof srv.addr) == -1) {
        syslog(LOG_ERR, "could not bind to %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));
        return 1;
    }

    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));
        return 1;
    }

    syslog(LOG_INFO, "listening on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));

    ev_signal_init(&sigint_listen, &mbm_ev_sigint, SIGINT);
    ev_io_init(&io_listen, &mbm_ev_conn_accept, sock, EV_READ);

    /* catch SIGINT */

    ev_signal_start(loop, &sigint_listen);
    ev_io_start(loop, &io_listen);

    ev_loop(loop, 0);

    return 0;
}

/** 
 * Called when sigint is recevied
 **/
static void
mbm_ev_sigint(EV_P_ ev_signal *w, int revents)
{
    int x;

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

    if (!(fp = fopen("mb-masterd.conf", "r"))) {
        syslog(LOG_ERR, "could not open configuration file");
        return 1;
    }

    while (fgets(in, 256, fp)) {
        if (in[0] == '#')
            continue;
        if (p = strchr(in, '=')) {
            len = p-in;
            while (isspace(in[len-1]))
                len--;
            do p++;
            while (isspace(*p));

            switch (len) {
                case 6:
                    if (strncmp(in, "listen", 6) == 0) {
                        if ((s = strchr(p, ':'))) {
                            srv.addr.sin_port = htons(atoi(s+1));
                            *s = '\0';
                        } else
                            srv.addr.sin_port = htons(MBM_DEFAULT_PORT);
                        srv.addr.sin_addr.s_addr = inet_addr(p);
                    }
                    break;

                default:
                    for (s=in; isalnum(*s) || *s == '_'; s++)
                        ;
                    *s = '\0';
                    syslog(LOG_ERR, "unknown option '%s'", in);
                    break;
            }
        }
    }

    fclose(fp);

    return 0;
}

