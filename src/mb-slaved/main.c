/*-
 * main.c
 * This file is part of mb-slaved
 *
 * Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * http://bithack.se/projects/methabot/
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "slave.h"
#include "conn.h"
#include "../libmetha/libev/ev.c"

int mbs_main();
int mbs_cleanup();
static void mbs_ev_sigint(EV_P_ ev_signal *w, int revents);
static int mbs_load_config();

struct slave srv;
static int mbs_master_login();
static int mbs_master_connect();
static void mbs_ev_master(EV_P_ ev_io *w, int revents);
static void mbs_ev_conn_accept(EV_P_ ev_io *w, int revents);

int
main(int argc, char **argv)
{
    int r=0;

    openlog("mb-slaved", 0, 0);
    syslog(LOG_INFO, "started");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    do {
        if ((r = mbs_load_config()) != 0)
            break;
        if ((r = mbs_master_connect()) != 0)
            break;
        if ((r = mbs_master_login()) != 0)
            break;
        if ((r = mbs_main()) != 0)
            break;
    } while (0);

    mbs_cleanup();
    closelog();
    return r;
}

int mbs_main()
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
        syslog(LOG_ERR, "could not bind to %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    if (listen(sock, 1024) == -1) {
        syslog(LOG_ERR, "could not listen on %s:%hd",
                inet_ntoa(srv.addr.sin_addr),
                ntohs(srv.addr.sin_port));
        return 1;
    }

    syslog(LOG_INFO, "listening on %s:%hd", inet_ntoa(srv.addr.sin_addr), ntohs(srv.addr.sin_port));

    ev_signal_init(&sigint_listen, &mbs_ev_sigint, SIGINT);
    ev_io_init(&io_listen, &mbs_ev_conn_accept, sock, EV_READ);
    ev_io_init(&master_io, &mbs_ev_master, srv.master_sock, EV_READ);

    /* catch SIGINT so we can close connections and clean up properly */
    ev_signal_start(loop, &sigint_listen);
    ev_io_start(loop, &io_listen);

    /** 
     * This loop will listen for new connections and data from
     * the master.
     **/
    ev_loop(loop, 0);

    ev_default_destroy();

    return 0;
}
/** 
 * Called when we have a new incoming connection
 **/
static void
mbs_ev_conn_accept(EV_P_ ev_io *w, int revents)
{

}

/** 
 * Called when a message is sent from the server
 **/
static void
mbs_ev_master(EV_P_ ev_io *w, int revents)
{
    if (revents | EV_ERROR) {
        ev_io_stop(EV_A_ w);
        ev_timer_init(&srv.master_timer, &mbs_ev_master_timer, 20.0f, 0.f);
        ev_timer_start(EV_A_ &srv.master_timer);
    }
}

/** 
 * Connect to the master server specified in mb-slaved.conf
 *
 * 0 on success
 **/
static int
mbs_master_connect()
{
    if ((srv.master_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return 1;

    srv.master.sin_family = AF_INET;

    if (connect(srv.master_sock, &srv.master, sizeof(struct sockaddr_in)) != 0) {
        syslog(SYS_ERR, "could not connect to master: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

/** 
 * Login to the master
 *
 * 0 on success
 **/
static int
mbs_master_login()
{
    int len;
    char str[128];
    if (strlen("AUTH slave ")+strlen(srv.user)
            +strlen(srv.pass)+3 >= 128) {
        syslog(SYS_ERR, "buffer exceeded");
        abort();
    }
    len = sprintf(str, "AUTH slave %s %s\n", srv.user, srv.pass);
    if (send(srv.master_sock, str, len, MSG_NOSIGNAL) == -1)
        return 1;

    len = recv(srv.master_sock, str, 127, MSG_NOSIGNAL);

    if (len != 0 && len != -1 && atoi(str) == 201) {
        syslog(SYS_INFO, "logged in to master");
        return 0
    }

    return 1;
}

int
mbs_cleanup()
{
    if (srv.user) free(srv.user);
    if (srv.pass) free(srv.pass);
}

static int
mbs_load_config()
{
    FILE *fp;
    char in[256];
    char *p;
    char *s;
    int len;

    if (!(fp = fopen("mb-slaved.conf", "r"))) {
        syslog(LOG_ERR, "could not open configuration file");
        return 1;
    }

    while (fgets(in, 256, fp)) {
        if (in[0] == '#')
            continue;
        p = in;
        in[strlen(in)-1] = '\0';
        while (isspace(*p))
            p++;
        if (p = strchr(p, '=')) {
            len = p-in;
            while (isspace(in[len-1]))
                len--;
            do p++; while (isspace(*p));

            int unknown = 1;
            switch (len) {
                case 5:
                    if (strncmp(in, "login", 5) == 0) {
                        if (!(s = strchr(p, ':'))) {
                            syslog(LOG_ERR, "login syntax incorrect (user:pwd)");
                            goto error;
                        }
                        *s = '\0';
                        srv.user = strdup(p);
                        srv.pass = strdup(s+1);
                    }
                    break;
                case 6:
                    if (strncmp(in, "master", 6) == 0) {
                        if ((s = strchr(p, ':'))) {
                            srv.master.sin_port = htons(atoi(s+1));
                            *s = '\0';
                        } else
                            srv.master.sin_port = htons(5304);
                        srv.master.sin_addr.s_addr = inet_addr(p);

                        unknown = 0;
                    } else if (strncmp(in, "listen", 6) == 0) {
                        if ((s = strchr(p, ':'))) {
                            srv.addr.sin_port = htons(atoi(s+1));
                            *s = '\0';
                        } else
                            srv.addr.sin_port = htons(MBS_DEFAULT_PORT);
                        srv.addr.sin_addr.s_addr = inet_addr(p);

                        unknown = 0;
                    }
                    break;
            }

            if (unknown) {
                for (s=in; isalnum(*s) || *s == '_'; s++)
                    ;
                *s = '\0';
                syslog(LOG_ERR, "unknown option '%s'", in);
                goto error;
            }
        } else {
            syslog(LOG_ERR, "missing argument for '%s'", in);
            goto error;
        }
    }

    fclose(fp);
    return 0;

error:
    fclose(fp);
    return 1;
}

