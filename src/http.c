
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)     *
 * Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>                   *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
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

/* Reject control characters (CR/LF etc.) so a crafted URL cannot inject
 * extra request headers or smuggle a second request. */
static int
has_ctrl (const char *s)
{
    for (; s && *s; s++)
	if ((unsigned char) *s < 0x20 || (unsigned char) *s == 0x7f)
	    return 1;
    return 0;
}

/* Percent-encode bytes that are illegal raw in a request-target (space,
 * control chars, high-bit bytes). A literal '%' passes through, so
 * already-encoded %XX sequences are not double-encoded. Returns -1 if the
 * result would not fit. */
static int
enc_target (const char *in, char *out, size_t outlen)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;

    for (; *in; in++) {
	unsigned char c = (unsigned char) *in;

	if (c <= 0x20 || c == 0x7f || c >= 0x80) {
	    if (o + 3 >= outlen)
		return -1;
	    out[o++] = '%';
	    out[o++] = hex[c >> 4];
	    out[o++] = hex[c & 0xf];
	} else {
	    if (o + 1 >= outlen)
		return -1;
	    out[o++] = (char) c;
	}
    }
    out[o] = '\0';
    return 0;
}

int
http_init (url_t * u)
{
    char req[BUFSIZ];
    char target[BUFSIZ];
    char hdr[MAX_HEADER_SIZE];
    int rlen, hlen = 0, nl = 0, status = 0;
    int defport = (u->proto == HTTPS) ? 443 : 80;
    /* an IPv6-literal host must be re-bracketed in the Host header */
    const char *ob = strchr (u->server, ':') ? "[" : "";
    const char *cb = *ob ? "]" : "";
    ssize_t off;

    if (has_ctrl (u->filename) || has_ctrl (u->server)) {
	fprintf (stderr, "vp: Invalid URL (control character in path or host)\n");
	return -1;
    }
    if (enc_target (u->filename, target, sizeof (target)) == -1) {
	fprintf (stderr, "vp: URL too long\n");
	return -1;
    }

    /* HTTP/1.1 request: CRLF line endings, Host (with the port when it is
     * not the scheme's default, per RFC 7230), and Connection: close so the
     * server closes the socket after the body instead of leaving it open
     * (which otherwise stalls the body read until the socket timeout). */
    if (u->port == defport)
	rlen = snprintf (req, sizeof (req),
	    "GET /%s HTTP/1.1\r\n"
	    "Host: %s%s%s\r\n"
	    "User-Agent: %s/%s\r\n"
	    "Connection: close\r\n"
	    "\r\n", target, ob, u->server, cb, PACKAGE, VERSION);
    else
	rlen = snprintf (req, sizeof (req),
	    "GET /%s HTTP/1.1\r\n"
	    "Host: %s%s%s:%d\r\n"
	    "User-Agent: %s/%s\r\n"
	    "Connection: close\r\n"
	    "\r\n", target, ob, u->server, cb, u->port,
	    PACKAGE, VERSION);
    if (rlen < 0 || (size_t) rlen >= sizeof (req)) {
	/* Truncated request would just stall the server; fail fast. */
	fprintf (stderr, "vp: URL too long\n");
	return -1;
    }
    if (net_write (u, req, (size_t) rlen) != (ssize_t) rlen)
	return -1;

    /* Read a header block byte-by-byte up to the blank line, leaving the
     * socket positioned exactly at the first body byte. Count newlines and
     * let CR ride along ignored, so CRLFCRLF, bare LFLF, and mixed endings
     * all terminate correctly without eating into the body. Interim 1xx
     * blocks (100 Continue, 103 Early Hints) precede the real response and
     * have no body; discard them and read on (RFC 9110), with a cap so a
     * 1xx-spamming server cannot spin us forever. */
    for (int interim = 0; ; interim++) {
	hlen = 0;
	nl = 0;
	while (nl < 2 && hlen < (int) sizeof (hdr) - 1)
	{
	    if (net_read (u, hdr + hlen, 1) != 1)
		return -1;	/* connection error or premature EOF */
	    if (hdr[hlen] == '\n')
		nl++;
	    else if (hdr[hlen] != '\r')
		nl = 0;
	    hlen++;
	}
	if (nl < 2)
	    return -1;		/* headers too large or malformed */
	hdr[hlen] = '\0';

	/* Status line: 2xx proceeds, 3xx is a redirect to chase (Location
	 * is picked up below), anything else fails so an error page
	 * (404/500) is never saved and rendered as if it were the image. */
	if (sscanf (hdr, "HTTP/%*d.%*d %d", &status) != 1)
	    return -1;
	if (status < 100 || status >= 200 || interim >= 5)
	    break;
    }
    if ((status < 200 || status >= 300) && !(status >= 300 && status < 400)) {
	fprintf (stderr, "vp: HTTP request failed: %d\n", status);
	return -1;
    }

    /* Parse framing headers (case-insensitive), starting after the status line. */
    {
	char *eol = strchr (hdr, '\n');
	off = eol ? (eol - hdr) + 1 : hlen;
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
	} else if (strncasecmp (line, "Location:", 9) == 0) {
	    const char *v = line + 9;
	    const char *vend = line + linelen;

	    while (v < vend && (*v == ' ' || *v == '\t'))
		v++;
	    while (vend > v && (vend[-1] == '\r' || vend[-1] == ' ' ||
		    vend[-1] == '\t'))
		vend--;
	    free (u->redirect);
	    u->redirect = malloc ((size_t) (vend - v) + 1);
	    if (u->redirect) {
		memcpy (u->redirect, v, (size_t) (vend - v));
		u->redirect[vend - v] = '\0';
	    }
	}
	line = eol ? eol + 1 : NULL;
    }

    if (status >= 300) {
	if (u->redirect == NULL || u->redirect[0] == '\0') {
	    fprintf (stderr, "vp: HTTP %d without a usable Location\n", status);
	    return -1;
	}
	return 1;		/* caller follows u->redirect */
    }
    return 0;
}
