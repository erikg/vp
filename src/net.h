
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)     *
 * Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>                   *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 ****************************************************************************/

#ifndef __NET_H_
#define __NET_H_

#include <sys/types.h>		/* ssize_t */
#include <time.h>		/* time_t */

#define HTTP 0x1
#define HTTPS 0x2

typedef struct {
    /*
     * file descriptors 
     */
    int file;
    int conn;
    /*
     * connection info 
     */
    int proto;			/* uh */
    char *server;		/* DNS name of server */
    int port;			/* numeric port value */
    char *filename;		/* file on server to get... */
    /*
     * mime info
     */
    char *mimetype;
    char *ext;
    /*
     * HTTP response framing (set by http_init, consumed by net_suck)
     */
    long content_length;	/* body length, or -1 if unknown */
    int chunked;		/* nonzero if Transfer-Encoding: chunked */
    char *redirect;		/* Location value when http_init returns 1 */
    time_t deadline;		/* wall-clock cutoff for the whole transfer */
    /*
     * TLS state (SSL* / SSL_CTX* when built with OpenSSL and the
     * connection is https; always present so the struct layout does not
     * depend on the build, always NULL for plain http)
     */
    void *ssl;
    void *ssl_ctx;
} url_t;

int net_is_url (char *name);
char *net_download (char *name);
void net_allow_bad_certs (void);
ssize_t net_read (url_t *u, void *buf, size_t len);
ssize_t net_write (url_t *u, const void *buf, size_t len);
void net_purge (char *file);
url_t *net_url (char *name);
void net_free_url (url_t *u);

#endif
