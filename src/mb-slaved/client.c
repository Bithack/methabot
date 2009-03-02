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

    if ((cl = malloc(sizeof(struct client)))) {
        now = time(0);
        a   = strlen(addr);
        p   = cl->token;

        for (x=0;x<18;) {
            if (x%5 == 4) {
                *(p+x) = '-';
                x++;
            } else {
                srand((long)(time(0)+(&addr)+x));
                char c = (((*(addr+7+(x%a))) << rand()%8) ^ now ^ (long)&addr) >> rand()%8;
                *(p+x) = ((c >> 4) & 15) + 'a';
                *(p+x+1) = ((c & 0x0F) & 15) + 'a' + rand()%8;
                x += 2;
            }
        }

        cl->addr.s_addr = inet_addr(addr);
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
            if (memcmp(srv.pending[x]->token, buf+5, 23) == 0) {
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

        if (this) {
            send(sock, "100 OK\n", 7, 0);
            
            /* enter main loop */

            mbs_client_free(this);
        } else
            send(sock, "200 Denied\n", 11, 0);
    } while (0);

    if (buf)
        free(buf);
    close(sock);
    return 0;
}
