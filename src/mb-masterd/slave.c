/*-
 * slave.c
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include "libev/ev.h"
#include "master.h"
#include "conn.h"
#include "nolp.h"

static int sl_status_command(nolp_t *no, char *buf, int size);
static int sl_status_parse(nolp_t *no, char *buf, int size);
static int read_token(EV_P_ struct conn *conn);

struct nolp_fn slave_commands[] = {
    {"STATUS", &sl_status_command},
    {0}
};


/**
 * prepare for receiving the status buffer
 **/
static int
sl_status_command(nolp_t *no, char *buf, int size)
{
    return nolp_expect(no, atoi(buf), &sl_status_parse);
}

/**
 * parse the status buffer, generate both XML and internal
 * representations
 *
 * xml syntax:
 * <client-list for="slave_name">
 *   <client id="sha1-hash">
 *     <user>username</user>
 *     <status>status</status>
 *     <address>address</address>
 *   </client>
 *   ...
 * </client-list>
 **/
static int
sl_status_parse(nolp_t *no, char *buf, int size)
{
    int x;
    int count = size/43;
    char *p;
    struct conn *conn = no->private;
    struct slave *sl = &srv.slaves[conn->slave_n];
    if (!(sl->clients = realloc(sl->clients, sizeof(struct client)*count)))
        return -1;
    if (!(p = (sl->xml.clients.buf = realloc(sl->xml.clients.buf,
                    count*(40+128+1+15+sizeof("<client id=\"\"><user></user><status></status><address></address></client>")-1)
                    +sizeof("<client-list for=\"\"></client-list>")-1))))
        return -1;
    sl->num_clients = count;
    p += sprintf(p, "<client-list for=\"%.64s\">","test_slave");// sl->name);
    for (x=0; x<count; x++) {
        memcpy(sl->clients[x].token, buf+(x*43), 40);
        sl->clients[x].state = atoi(buf+(x*43)+41);
        p+=sprintf(p, 
                "<client id=\"%.40s\">"
                "<user>%.64s</user>"
                "<status>1</status>"
                "<address>%s</address>"
                "</client>",
                sl->clients[x].token, "test", "127.0.0.1");
    }
    p += sprintf(p, "</client-list>");

    x = p-sl->xml.clients.buf;
    sl->xml.clients.buf = realloc(sl->xml.clients.buf, x);
    sl->xml.clients.sz = x;

    return 0;
}

