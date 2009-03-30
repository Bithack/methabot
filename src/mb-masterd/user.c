/*-
 * user.c
 * This file is part of mb-masterd
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
 * http://metha-sys.org/
 * http://bithack.se/projects/methabot/
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
 */

#include "libev/ev.h"
#include "master.h"
#include "nolp.h"

#include <stdio.h>

static int user_list_clients_command(nolp_t *no, char *buf, int size);
static int user_list_slaves_command(nolp_t *no, char *buf, int size);
static int user_slave_info_command(nolp_t *no, char *buf, int size);
static int user_client_info_command(nolp_t *no, char *buf, int size);
static int user_show_config_command(nolp_t *no, char *buf, int size);
static int user_log_command(nolp_t *no, char *buf, int size);
static int user_add_command(nolp_t *no, char *buf, int size);

struct nolp_fn user_commands[] = {
    {"LIST-SLAVES", &user_list_slaves_command},
    {"LIST-CLIENTS", user_list_clients_command},
    {"SLAVE-INFO", user_slave_info_command},
    {"CLIENT-INFO", user_client_info_command},
    {"SHOW-CONFIG", user_show_config_command},
    {"LOG", user_log_command},
    {"ADD", user_add_command},
    {0},
};

#define MSG100 "100 OK\n"
#define MSG203 "203 Not found\n"
#define MSG300 "300 Internal Error\n"

/** 
 * The LIST-CLIENTS command requests a list of clients
 * connected to the given slave. Syntax:
 * LIST-CLIENTS <slave>\n
 * 'slave' should be the iid of a slave
 **/
static int
user_list_clients_command(nolp_t *no, char *buf, int size)
{
    int  x;
    int  id;
    char reply[32];
    struct conn *conn = (struct conn*)no->private;

    id = atoi(buf);

    for (x=0; ; x++) {
        if (x == srv.num_slaves) {
            send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
            break;
        }
        struct slave *sl = &srv.slaves[x];
        if (sl->id == id) {
            int len = sprintf(reply, "100 %d\n", sl->xml.clients.sz);
            send(conn->sock, reply, len, 0);
            send(conn->sock, sl->xml.clients.buf, sl->xml.clients.sz, 0);
            break;
        }
    }

    return 0;
}

/** 
 * LIST-SLAVES returns a list of all slaves in XML format, 
 * including their client count. Syntax:
 * LIST-SLAVES 0\n
 * currently, the second argument is unused and should be 
 * set to 0.
 **/
static int
user_list_slaves_command(nolp_t *no, char *buf, int size)
{
    char reply[32];
    struct conn *conn = (struct conn*)no->private;
    int len = sprintf(reply, "100 %d\n", srv.xml.slave_list.sz);
    send(conn->sock, reply, len, 0);
    send(conn->sock, srv.xml.slave_list.buf, srv.xml.slave_list.sz, 0);

    return 0;
}

/** 
 * The SLAVE-INFO outputs information about a slave, such
 * as address and num connected clients, in XML format.
 * Syntax:
 * SLAVE-INFO <slave>\n
 * 'slave' should be the NAME of a slave
 **/
static int
user_slave_info_command(nolp_t *no, char *buf, int size)
{
    int x;
    int id;
    char tmp[32];
    char reply[
        sizeof("<slave-info for=\"\"><address></address></slave-info>")-1
        +64+20+16];
    struct conn  *conn = (struct conn*)no->private;
    struct slave *sl = 0;

    id = atoi(buf);
    for (x=0; x<srv.num_slaves; x++) {
        if (srv.slaves[x].id == id) {
            sl = &srv.slaves[x];
            break;
        }
    }
    if (!sl) {
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }
    int len =
        sprintf(reply,
            "<slave-info for=\"%s-%d\">"
              "<address>%s</address>"
            "</slave-info>",
            sl->name,
            sl->id,
            inet_ntoa(conn->addr.sin_addr));
    x = sprintf(tmp, "100 %d\n", len);
    send(conn->sock, tmp, x, 0);
    send(conn->sock, reply, len, 0);
    return 0;
}

/** 
 * The CLIENT-INFO outputs information about a client
 * in XML format. Syntax:
 * CLIENT-INFO <client>\n
 * 'client' should be the NAME of a client
 **/
static int
user_client_info_command(nolp_t *no, char *buf, int size)
{
    int x, y;
    int  found;
    int  sz;
    char out[256];
    struct client *c;
    struct slave  *sl;
    struct conn   *conn;

    if (size != 40)
        return -1; /* return -1 to close the connection */

    found = 0;
    c = 0;
    conn = (struct conn*)no->private;

    /* search through all slaves for a client matching the 
     * given hash */
    for (x=0; x<srv.num_slaves; x++) {
        for (y=0; y<srv.slaves[x].num_clients; y++) {
            if (memcmp(srv.slaves[x].clients[y].token, buf, 40) == 0) {
                c = &srv.slaves[x].clients[y];
                sl = &srv.slaves[x];
                found = 1;
                break;
            }
        }
        if (found)
            break;
    }
    if (!c) {
        /* send 203 not found message if the client was not
         * found */
        send(conn->sock, MSG203, sizeof(MSG203)-1, 0);
        return 0;
    }

    sz = sprintf(out, 
            "<client id=\"%.40s\">"
              "<user>%.64s</user>"
              "<slave>%.64s-%d</slave>"
              "<status>%d</status>"
              "<address>%.15s</address>"
            "</client>",
            c->token,
            c->user,
            sl->name, sl->id,
            (c->status & 1),
            c->addr);
    x = sprintf(out+sz, "100 %d\n", sz);

    send(conn->sock, out+sz, x, 0);
    send(conn->sock, out, sz, 0);
    return 0;
}

static int
user_show_config_command(nolp_t *no, char *buf, int size)
{
    char           out[64];
    struct conn   *conn;
    int            x;

    conn = (struct conn *)no->private;
    x = sprintf(out, "100 %d\n", srv.config_sz);
    send(conn->sock, out, x, 0);
    send(conn->sock, srv.config_buf, srv.config_sz, 0);
    return 0;
}

static int
user_log_command(nolp_t *no, char *buf, int size)
{
    return 0;
}

/** 
 * called when the ADD request is performed by
 * a user. ADD is used to add URLs. They will be 
 * added with a last-crawled date of 00-00-00 and
 * thus be put at the front of the queue.
 **/
static int
user_add_command(nolp_t *no, char *buf, int size)
{
    char q[size+96];
    int  len;
    int  x;
    /* make sure there's no single-quote that can 
     * risk injecting SQL */
    for (x=0; x<size; x++)
        if (buf[x] == '\'')
            buf[x] = '_';
    len = sprintf(q, "INSERT INTO _url (url, hash, date) "
                     "VALUES ('%s', SHA1(url), '00-00-00 00:00:00')", buf);
    if (mysql_real_query(srv.mysql, q, len) != 0) {
        send(no->fd, MSG300, sizeof(MSG300)-1, 0);
        return -1;
    }

    send(no->fd, MSG100, sizeof(MSG100)-1, 0);
    return 0;
}

