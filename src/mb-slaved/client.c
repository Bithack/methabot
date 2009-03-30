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
#include <mysql/mysql.h>

void mbs_client_main(struct client *this);

/** 
 * Create a new client with a random login-token
 *
 * addr should be the clients IP-address
 **/
struct client*
mbs_client_create(const char *addr, const char *user)
{
    struct client* cl;
    time_t now;
    int a, x;
    char *p;
    char q[128];
    int qlen;

    MYSQL_RES *res;
    MYSQL_ROW *row;

    if ((cl = malloc(sizeof(struct client)))) {
        if ((cl->addr.s_addr = inet_addr(addr)) == (in_addr_t)-1
                || !(cl->user = strdup(user))) {
            syslog(LOG_ERR, "invalid address/user");
            free(cl);
            return 0;
        }
        qlen = sprintf(q, "SELECT SHA1(CONCAT(CONCAT(CONCAT('%s', NOW()), RAND()), RAND()))", addr);
        if (mysql_real_query(srv.mysql, q, qlen) != 0
                || !(res = mysql_store_result(srv.mysql))
                || !(mysql_num_rows(res))
                || !(row = mysql_fetch_row(res))
                ) {
            free(cl);
            return 0;
        }
        if (row[0])
            memcpy(cl->token, row[0], 40);
        else
            return 0;

        mysql_free_result(res);

        /** 
         * add the client to the _client table which will give us an 
         * integer identifier to be used when adding URLs to the log
         * and save target urls
         **/
        qlen = sprintf(q, "INSERT INTO _client (token) VALUES ('%.40s');", cl->token);
        if (mysql_real_query(srv.mysql, q, qlen) != 0) {
            free(cl);
            return 0;
        }
        cl->id = (long)mysql_insert_id(srv.mysql);

        syslog(LOG_INFO,"new client %d:'%.6s...' from %s", (int)cl->id, cl->token, addr);
    }

    return cl;
}

void
mbs_client_free(struct client *cl)
{
    free(cl->user);
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

        this->running = 1;

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

