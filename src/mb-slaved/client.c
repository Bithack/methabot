/*-
 * client.c
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

#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include "slave.h"
#include "client.h"
#include <pthread.h>

void mbs_client_main(struct client *this);

/** 
 * Create a new client with a random login-token
 *
 * addr should be the clients IP-address
 **/
struct client*
mbs_client_create(const char *addr)
{
    struct client* cl;
    time_t now;
    int a, x;
    char *p;
    char q[512];

    if ((cl = malloc(sizeof(struct client)))) {
        if ((cl->addr.s_addr = inet_addr(addr)) == (in_addr_t)-1) {
            syslog(LOG_ERR, "invalid address");
            free(cl);
            return 0;
        }
        memcpy(cl->token, "d0be2dc421be4fcd0172e5afceea3970e2f3d940", 40);

        /*
        qlen = sprintf(q, "SELECT CONCAT(CONCAT(CONCAT('%s', NOW()), RAND()), RAND())", addr);
        if (mysql_real_query(srv.mysql, q, qlen) == 0)
        */
    }

    return cl;
}

void
mbs_client_free(struct client *cl)
{
    free(cl);
}

void *
mbs_client_init(void *in)
{
    int sock = (int)in;
    int n;
    int x;
    char *buf;
    struct client *this = 0;

    do {
        if (!(buf = malloc(256)))
            break;

        /* first authorize this client connection */
        if ((n = sock_getline(sock, buf, 255)) <= 0)
            break;

        if (n != 6+TOKEN_SIZE || memcmp(buf, "AUTH ", 5) != 0)
            break;

        pthread_mutex_lock(&srv.pending_lk);
        for (x=0; x<srv.num_pending; x++) {
            if (memcmp(srv.pending[x]->token, buf+5, TOKEN_SIZE) == 0) {
                this = srv.pending[x];
                if (x != srv.num_pending-1)
                    srv.pending[x] = srv.pending[srv.num_pending-1];
                srv.num_pending --;
                break;
            }
        }
        pthread_mutex_unlock(&srv.pending_lk);

        free(buf);
        buf = 0;

        if (!this) {
            send(sock, "200 Denied\n", 11, 0);
            break;
        }
        send(sock, "100 OK\n", 7, 0);

        /* move 'this' into the global list of clients
         * at srv.clients */
        pthread_mutex_lock(&srv.clients_lk);
        if (!(srv.clients = realloc(srv.clients, (srv.num_clients+1)*sizeof(struct client*))))
            abort();
        srv.clients[srv.num_clients] = this;
        srv.num_clients++;
        pthread_mutex_unlock(&srv.clients_lk);
        
        /* notify the main thread that we chagned the client list,
         * the slave in turn will update the master with the new 
         * list */
        ev_async_send(EV_DEFAULT_ &srv.client_status);

        /* enter main loop */
        mbs_client_main(this);
    } while (0);

    if (buf)
        free(buf);
    close(sock);

    /* remove this client from the client list, 
     * if it's been added to it earlier */
    if (this) {
        pthread_mutex_lock(&srv.clients_lk);
        for (x=0; x<srv.num_clients; x++) {
            if (srv.clients[x] == this) {
                if (x != srv.num_clients-1) {
                    srv.clients[x] = srv.clients[srv.num_clients-1];
                }
                if (srv.num_clients == 1) {
                    free(srv.clients);
                    srv.clients = 0;
                } else if (!(srv.clients = realloc(srv.clients, (srv.num_clients-1)*sizeof(struct client*))))
                        abort();
                srv.num_clients--;
                break;
            }
        }
        pthread_mutex_unlock(&srv.clients_lk);
        ev_async_send(EV_DEFAULT_ &srv.client_status);
        mbs_client_free(this);
    }

    return 0;
}

void
mbs_client_main(struct client *this)
{
    struct ev_loop *loop;

    /*
    if (!(loop = ev_loop_new(EVFLAG_AUTO)))
        return;

    ev_loop(loop, 0);
    ev_loop_destroy(loop);
    */
    sleep(1000);
}

