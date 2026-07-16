
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)     *
 * Copyright (C) 2001-2025 Erik Greenwald <erik@elfga.com>                   *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "http.h"

#define MAX_HEADER_SIZE 8192	/* 8KB header block limit */

int
http_init (url_t * u)
{
    char req[BUFSIZ];
    char hdr[MAX_HEADER_SIZE];
    int hlen = 0, e = 0, status = 0;
    ssize_t off;

    /* HTTP/1.1 request: CRLF line endings, Host, and Connection: close so the
     * server closes the socket after the body instead of leaving it open
     * (which otherwise stalls the body read until the socket timeout). */
    snprintf (req, sizeof (req),
	"GET /%s HTTP/1.1\r\n"
	"Host: %s\r\n"
	"User-Agent: %s/%s\r\n"
	"Connection: close\r\n"
	"\r\n", u->filename, u->server, PACKAGE, VERSION);
    if (write (u->conn, req, strlen (req)) != (ssize_t) strlen (req))
	return -1;

    /* Read the header block byte-by-byte up to the blank line (CRLFCRLF), so
     * the socket is left positioned exactly at the first body byte. */
    while (e < 4 && hlen < (int) sizeof (hdr) - 1)
    {
	if (read (u->conn, hdr + hlen, 1) != 1)
	    return -1;		/* connection error or premature EOF */
	if (hdr[hlen] == '\n' || hdr[hlen] == '\r')
	    e++;
	else
	    e = 0;
	hlen++;
    }
    if (e < 4)
	return -1;		/* headers too large or malformed */
    hdr[hlen] = '\0';

    /* Status line: reject anything that is not 2xx so an error page (404/500)
     * is never saved and rendered as if it were the image. */
    if (sscanf (hdr, "HTTP/%*d.%*d %d", &status) != 1)
	return -1;
    if (status < 200 || status >= 300) {
	fprintf (stderr, "HTTP request failed: %d\n", status);
	return -1;
    }

    /* Parse framing headers (case-insensitive), starting after the status line. */
    off = 0;
    {
	char *nl = strchr (hdr, '\n');
	off = nl ? (nl - hdr) + 1 : hlen;
    }
    for (char *line = hdr + off; line && *line; ) {
	char *eol = strchr (line, '\n');
	size_t linelen = eol ? (size_t) (eol - line) : strlen (line);

	if (strncasecmp (line, "Content-Length:", 15) == 0) {
	    long cl = strtol (line + 15, NULL, 10);
	    u->content_length = (cl >= 0) ? cl : -1;
	} else if (strncasecmp (line, "Transfer-Encoding:", 18) == 0) {
	    for (char *p = line + 18; p < line + linelen; p++) {
		if (strncasecmp (p, "chunked", 7) == 0) {
		    u->chunked = 1;
		    break;
		}
	    }
	}
	line = eol ? eol + 1 : NULL;
    }
    return 0;
}
