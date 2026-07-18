
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include <limits.h>

#ifdef WIN32
# include <winsock.h>
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <sys/uio.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#ifdef HAVE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/x509.h>
#endif

#include "http.h"
#include "net.h"

#ifndef HAVE_MKSTEMPS
#include <time.h>

	/*
	 * Fallback mkstemps() for platforms lacking one, matching glibc
	 * semantics: the 6 'X's immediately preceding the suffix are replaced
	 * with random characters, and the file is created atomically with
	 * O_EXCL (retrying on collision) so it cannot be pre-created as a
	 * symlink by a local attacker.
	 */
static unsigned int rng_state;

static char
randchar (void)
{
    static const char set[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    /* xorshift32 - no dependency on / no clobbering of libc rand() */
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return set[x % (sizeof (set) - 1)];
}

int
mkstemps (char *template, int suffixlen)
{
    size_t len;
    char *x;
    int i, tries;

    if (template == NULL || suffixlen < 0) {
	errno = EINVAL;
	return -1;
    }
    len = strlen (template);
    /* Need six 'X's sitting immediately before the suffix. */
    if ((size_t) suffixlen > len || len - (size_t) suffixlen < 6) {
	errno = EINVAL;
	return -1;
    }
    x = template + len - (size_t) suffixlen - 6;
    for (i = 0; i < 6; i++) {
	if (x[i] != 'X') {
	    errno = EINVAL;
	    return -1;
	}
    }

    /* Seed once per process; later calls continue the stream instead of
     * replaying the same names within the same second. */
    if (rng_state == 0)
	rng_state =
	    (unsigned int) (time (NULL) ^ ((unsigned int) getpid () << 16));
    if (rng_state == 0)
	rng_state = 0x1234567u;

    for (tries = 0; tries < 4096; tries++) {
	int f;

	for (i = 0; i < 6; i++)
	    x[i] = randchar ();
	f = open (template, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (f >= 0)
	    return f;
	if (errno != EEXIST)
	    return -1;
    }
    errno = EEXIST;
    return -1;
}

#endif

int
net_is_url (char *name)
{
    /* https is claimed even in a build without TLS support, so those URLs
     * get a clear "built without https support" message instead of the
     * misleading "not a readable file". */
    return !strncasecmp (name, "http://", 7) ||
	!strncasecmp (name, "https://", 8);
}

void
net_free_url (url_t *u)
{
    if (u) {
#ifdef HAVE_OPENSSL
	if (u->ssl) {
	    SSL_shutdown ((SSL *) u->ssl);
	    SSL_free ((SSL *) u->ssl);
	}
	if (u->ssl_ctx)
	    SSL_CTX_free ((SSL_CTX *) u->ssl_ctx);
#endif
	if (u->server) free (u->server);
	if (u->filename) free (u->filename);
	if (u->ext) free (u->ext);
	if (u->mimetype) free (u->mimetype);
	if (u->redirect) free (u->redirect);
	if (u->file >= 0) close (u->file);
	if (u->conn >= 0) close (u->conn);
	free (u);
    }
}

/*
 * Read/write on the connection, through TLS when the connection is https.
 * net_read returns bytes read, 0 at EOF, -1 on error, matching read(2).
 */
ssize_t
net_read (url_t * u, void *buf, size_t len)
{
#ifdef HAVE_OPENSSL
    if (u->ssl) {
	int n;

	if (len > INT_MAX)
	    len = INT_MAX;
	n = SSL_read ((SSL *) u->ssl, buf, (int) len);
	if (n > 0)
	    return n;
	switch (SSL_get_error ((SSL *) u->ssl, n)) {
	    case SSL_ERROR_ZERO_RETURN:
		return 0;	/* clean TLS shutdown */
	    case SSL_ERROR_SYSCALL:
		/* Close without close_notify: common in the wild. Treat
		 * as EOF and let Content-Length framing catch actual
		 * truncation. */
		if (ERR_peek_error () == 0 && errno == 0)
		    return 0;
		return -1;
	    default:
		return -1;
	}
    }
#endif
    return read (u->conn, buf, len);
}

ssize_t
net_write (url_t * u, const void *buf, size_t len)
{
#ifdef HAVE_OPENSSL
    if (u->ssl) {
	int n;

	if (len > INT_MAX)
	    len = INT_MAX;
	n = SSL_write ((SSL *) u->ssl, buf, (int) len);
	return (n > 0) ? n : -1;
    }
#endif
    return write (u->conn, buf, len);
}

/* Set socket timeouts to prevent hanging connections */
static int
set_socket_timeout(int sockfd, int timeout_sec)
{
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
	return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
	return -1;
    }
    return 0;
}

url_t *
net_url (char *name)
{
    url_t *u;
    char *n;
    int tls;
    size_t schemelen;

    tls = (strncasecmp (name, "https://", 8) == 0);
    schemelen = tls ? strlen ("https://") : strlen ("http://");
    n = name + schemelen;

    /* Find the '/' character safely */
    char *slash = strchr(n, '/');
    if (slash == NULL) {
	return NULL;  /* Invalid URL - no path component */
    }

    /* Temporarily null-terminate the server part */
    *slash = 0;
    n = slash + 1;
    u = (url_t *) malloc (sizeof (url_t));
    if (u == NULL) {
	*slash = '/';
	return NULL;
    }

    /* Initialize every field before anything can fail, so net_free_url()
     * on the error path below never frees or closes garbage. */
    u->port = tls ? 443 : 80;
    u->proto = tls ? HTTPS : HTTP;
    u->mimetype = NULL;
    u->redirect = NULL;
    u->ssl = NULL;
    u->ssl_ctx = NULL;
    u->file = -1;
    u->conn = -1;
    u->content_length = -1;
    u->chunked = 0;

    u->server = strdup (name + schemelen);
    u->filename = strdup (n);

    /* Extension hint for the temp file (SDL_image sniffs content, but the
     * suffix helps): everything after the last '.' of the path's basename,
     * ignoring any ?query/#fragment. No dot in the basename means no hint.
     * Never take a raw tail slice - a '/' in it breaks the mkstemps path. */
    {
	const char *base, *q, *dot, *p;

	base = strrchr (n, '/');
	base = base ? base + 1 : n;
	q = base + strcspn (base, "?#");
	dot = NULL;
	for (p = base; p < q; p++)
	    if (*p == '.')
		dot = p;
	if (dot && q - dot > 1) {
	    size_t elen = (size_t) (q - dot - 1);

	    u->ext = malloc (elen + 1);
	    if (u->ext) {
		memcpy (u->ext, dot + 1, elen);
		u->ext[elen] = '\0';
	    }
	} else
	    u->ext = strdup ("");
    }

    /* Put the '/' back: the caller's string is argv, and it is displayed
     * (window title, OSD, -l output) after we return. */
    *slash = '/';

    /* Check for strdup failures */
    if (!u->server || !u->filename || !u->ext) {
	net_free_url (u);
	return NULL;
    }

    /* Optional :port in the authority (our copy, argv already restored). */
    {
	char *colon = strchr (u->server, ':');

	if (colon) {
	    char *end;
	    long p = strtol (colon + 1, &end, 10);

	    if (end == colon + 1 || *end != '\0' || p < 1 || p > 65535) {
		fprintf (stderr, "Invalid port in URL: %s\n", name);
		net_free_url (u);
		return NULL;
	    }
	    u->port = (int) p;
	    *colon = '\0';
	}
    }

    return u;
}

/* -k/--insecure: accept https certificates that fail verification. The
 * flag lives here (not in vp.c's state machine) so a no-TLS build parses
 * it as a harmless no-op without any #ifdef in the option handler. */
static int allow_bad_certs = 0;

void
net_allow_bad_certs (void)
{
    allow_bad_certs = 1;
}

#ifdef HAVE_OPENSSL
/*
 * Wrap an already-connected socket in TLS: server certificate verified
 * against the system trust store, hostname checked against the URL's,
 * SNI sent (shared-hosting servers need it to pick the right cert).
 * With -k verification failures are reported but not fatal.
 */
static int
net_tls_start (url_t * u)
{
    SSL_CTX *ctx;
    SSL *ssl;

    if ((ctx = SSL_CTX_new (TLS_client_method ())) == NULL)
	return -1;
    SSL_CTX_set_verify (ctx,
	allow_bad_certs ? SSL_VERIFY_NONE : SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_default_verify_paths (ctx) != 1 && !allow_bad_certs) {
	fprintf (stderr, "vp: no system certificate store found\n");
	SSL_CTX_free (ctx);
	return -1;
    }
    if ((ssl = SSL_new (ctx)) == NULL) {
	SSL_CTX_free (ctx);
	return -1;
    }
    if (SSL_set_tlsext_host_name (ssl, u->server) != 1 ||
	SSL_set1_host (ssl, u->server) != 1 ||
	SSL_set_fd (ssl, u->conn) != 1) {
	SSL_free (ssl);
	SSL_CTX_free (ctx);
	return -1;
    }
    if (SSL_connect (ssl) != 1) {
	long vr = SSL_get_verify_result (ssl);

	if (vr != X509_V_OK)
	    fprintf (stderr, "vp: %s: certificate verification failed: %s\n",
		u->server, X509_verify_cert_error_string (vr));
	else {
	    unsigned long e = ERR_peek_last_error ();
	    const char *reason = e ? ERR_reason_error_string (e) : NULL;

	    fprintf (stderr, "vp: TLS handshake with %s failed: %s\n",
		u->server, reason ? reason : "unknown error");
	}
	SSL_free (ssl);
	SSL_CTX_free (ctx);
	return -1;
    }
    /* Verification still ran under SSL_VERIFY_NONE; say what -k waved
     * through so the acceptance is at least an informed one. */
    if (allow_bad_certs) {
	long vr = SSL_get_verify_result (ssl);

	if (vr != X509_V_OK)
	    fprintf (stderr,
		"vp: %s: accepting untrusted certificate (-k): %s\n",
		u->server, X509_verify_cert_error_string (vr));
    }
    u->ssl = ssl;
    u->ssl_ctx = ctx;
    return 0;
}
#endif

static int
net_connect (url_t * u)
{
    struct sockaddr_in s;
    struct sockaddr *ss = (struct sockaddr *)&s;
    struct hostent *h;

#ifndef HAVE_OPENSSL
    if (u->proto == HTTPS) {
	fprintf (stderr,
	    "vp: %s: this vp was built without https support\n", u->server);
	return -1;
    }
#endif

    memset (&s, 0, sizeof (s));
    if ((u->conn = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
	perror ("vp:net.c:net_connect:socket");
	return -1;
    }
    if ((h = gethostbyname (u->server)) == NULL)
    {
	/* gethostbyname reports through h_errno; perror would print
	 * whatever stale errno happened to be lying around. */
	fprintf (stderr, "vp: cannot resolve %s: %s\n",
	    u->server, hstrerror (h_errno));
	close (u->conn);
	u->conn = -1;
	return -1;
    }
    s.sin_family = AF_INET;
    s.sin_port = htons (u->port);
    s.sin_addr = *((struct in_addr *)h->h_addr_list[0]);
    /* Set socket timeouts before connecting */
    if (set_socket_timeout(u->conn, 30) == -1) {
	perror ("vp:net.c:net_connect:set_socket_timeout");
	/* Continue anyway - timeouts are not critical for basic functionality */
    }

    if (connect (u->conn, ss, sizeof (struct sockaddr)) == -1)
    {
	perror ("vp:net.c:net_connect:connect");
	close (u->conn);
	u->conn = -1;
	return -1;
    }
#ifdef HAVE_OPENSSL
    if (u->proto == HTTPS && net_tls_start (u) == -1) {
	close (u->conn);
	u->conn = -1;
	return -1;
    }
#endif
    return 0;
}

#define MAX_DOWNLOAD_SIZE ((size_t)100 * 1024 * 1024)	/* 100MB limit */

/*
 * Buffered reader over the connection socket. http_init leaves the socket
 * positioned exactly at the first body byte, so this owns it from there.
 */
typedef struct {
    url_t *u;
    unsigned char buf[BUFSIZ];
    size_t pos, len;
} breader_t;

/* Return next byte, or -1 on EOF/error. */
static int
br_getc (breader_t * b)
{
    if (b->pos >= b->len) {
	ssize_t n = net_read (b->u, b->buf, sizeof (b->buf));
	if (n <= 0)
	    return -1;
	b->pos = 0;
	b->len = (size_t) n;
    }
    return b->buf[b->pos++];
}

/* Read up to want bytes into dst; return count (0 at EOF), -1 on error. */
static ssize_t
br_read (breader_t * b, unsigned char *dst, size_t want)
{
    size_t got = 0;

    while (got < want) {
	if (b->pos >= b->len) {
	    ssize_t n = net_read (b->u, b->buf, sizeof (b->buf));
	    if (n < 0)
		return -1;
	    if (n == 0)
		break;		/* EOF */
	    b->pos = 0;
	    b->len = (size_t) n;
	}
	size_t avail = b->len - b->pos;
	size_t take = (want - got < avail) ? (want - got) : avail;
	memcpy (dst + got, b->buf + b->pos, take);
	b->pos += take;
	got += take;
    }
    return (ssize_t) got;
}

/* Write len bytes to the download file, enforcing the size cap. */
static int
sink_write (url_t * u, const unsigned char *data, size_t len, size_t *total)
{
    *total += len;
    if (*total > MAX_DOWNLOAD_SIZE) {
	fprintf (stderr, "Download size limit exceeded (%zu bytes)\n",
	    MAX_DOWNLOAD_SIZE);
	return -1;
    }
    if (write (u->file, data, len) != (ssize_t) len)
	return -1;
    return 0;
}

/* Decode a Transfer-Encoding: chunked body straight to the file. */
static int
net_suck_chunked (url_t * u, breader_t * br)
{
    size_t total = 0;

    for (;;) {
	char line[64];
	int i = 0, c;

	/* chunk-size line: hex, possibly with ;extensions, ending CRLF */
	while ((c = br_getc (br)) != -1 && c != '\n') {
	    if (c != '\r' && i < (int) sizeof (line) - 1)
		line[i++] = (char) c;
	}
	if (c == -1)
	    return -1;
	line[i] = '\0';

	/* An unparseable size line must not read as "0" - that is the
	 * last-chunk marker and would pass truncation off as success. */
	char *end;
	long chunk = strtol (line, &end, 16);
	if (end == line || chunk < 0)
	    return -1;
	if (chunk == 0)
	    break;		/* last chunk */

	while (chunk > 0) {
	    unsigned char tmp[BUFSIZ];
	    size_t want = (chunk < (long) sizeof (tmp)) ? (size_t) chunk : sizeof (tmp);
	    ssize_t got = br_read (br, tmp, want);
	    if (got <= 0)
		return -1;	/* truncated */
	    if (sink_write (u, tmp, (size_t) got, &total) == -1)
		return -1;
	    chunk -= got;
	}

	/* consume the CRLF trailing the chunk data */
	c = br_getc (br);
	if (c == '\r')
	    c = br_getc (br);
	if (c == -1)
	    return -1;
    }
    return 0;
}

static int
net_suck (url_t * u)
{
    breader_t br;
    size_t total = 0;
    long remaining;

    br.u = u;
    br.pos = br.len = 0;

    if (u->chunked)
	return net_suck_chunked (u, &br);

    /* identity encoding: bounded by Content-Length, else read until close */
    remaining = u->content_length;	/* -1 == until EOF */
    for (;;) {
	unsigned char tmp[BUFSIZ];
	size_t want = sizeof (tmp);
	ssize_t got;

	if (remaining >= 0) {
	    if (remaining == 0)
		break;
	    if ((long) want > remaining)
		want = (size_t) remaining;
	}
	got = br_read (&br, tmp, want);
	if (got < 0)
	    return -1;
	if (got == 0) {
	    /* EOF before the declared Content-Length is a truncated
	     * download, not a success. */
	    if (remaining > 0)
		return -1;
	    break;		/* EOF (connection close) */
	}
	if (sink_write (u, tmp, (size_t) got, &total) == -1)
	    return -1;
	if (remaining >= 0)
	    remaining -= got;
    }
    return 0;
}

#define MAX_REDIRECTS 5

/*
 * Resolve a Location header value against the URL that produced it into a
 * malloc'd absolute URL, or NULL (with a message) if it cannot be
 * followed. Handles absolute, protocol-relative (//host/x), absolute-path
 * (/x), and relative (x) forms; the last three inherit the base's scheme.
 */
static char *
resolve_location (const url_t *base, const char *loc)
{
    const char *scheme = (base->proto == HTTPS) ? "https" : "http";
    char *out = NULL;
    size_t n;

#ifndef HAVE_OPENSSL
    if (strncasecmp (loc, "https://", 8) == 0) {
	fprintf (stderr, "Redirected to %s - this vp was built without "
	    "https support, stopping\n", loc);
	return NULL;
    }
#endif
    if (strncasecmp (loc, "http://", 7) == 0 ||
	strncasecmp (loc, "https://", 8) == 0) {
	out = strdup (loc);
    } else if (loc[0] == '/' && loc[1] == '/') {
	n = strlen (scheme) + 1 + strlen (loc) + 1;
	if ((out = malloc (n)) != NULL)
	    snprintf (out, n, "%s:%s", scheme, loc);
    } else if (loc[0] == '/') {
	/* absolute path on the same authority */
	n = strlen (scheme) + strlen (base->server) + strlen (loc) + 32;
	if ((out = malloc (n)) != NULL)
	    snprintf (out, n, "%s://%s:%d%s", scheme, base->server,
		base->port, loc);
    } else {
	/* relative to the directory of the current path */
	const char *slash = strrchr (base->filename, '/');
	size_t dirlen = slash ? (size_t) (slash - base->filename) + 1 : 0;

	n = strlen (scheme) + strlen (base->server) + dirlen +
	    strlen (loc) + 32;
	if ((out = malloc (n)) != NULL)
	    snprintf (out, n, "%s://%s:%d/%.*s%s", scheme, base->server,
		base->port, (int) dirlen, base->filename, loc);
    }

    /* net_url needs a path slash after the authority; a bare
     * "http://host" target gets one appended. */
    if (out) {
	const char *auth = out +
	    (strncasecmp (out, "https://", 8) == 0 ? 8 : 7);

	if (strchr (auth, '/') == NULL) {
	    n = strlen (out) + 2;
	    char *slashed = malloc (n);

	    if (slashed)
		snprintf (slashed, n, "%s/", out);
	    free (out);
	    out = slashed;
	}
    }
    return out;
}

char *
net_download (char *name)
{
    char *filename;
    int len, hops;
    url_t *url = NULL;
    char *loc = NULL;		/* malloc'd URL of the current hop, if any */

    /* Chase redirects until a 2xx leaves the body ready on the socket. */
    for (hops = 0; ; hops++) {
	int r;

	if ((url = net_url (loc ? loc : name)) == NULL ||
	    net_connect (url) == -1) {
	    if (url) net_free_url (url);
	    free (loc);
	    return NULL;
	}
	r = http_init (url);
	if (r == 0)
	    break;
	if (r == 1 && hops < MAX_REDIRECTS) {
	    char *next = resolve_location (url, url->redirect);

	    net_free_url (url);
	    free (loc);
	    loc = next;
	    if (loc != NULL)
		continue;
	    return NULL;	/* resolve_location said why */
	}
	if (r == 1)
	    fprintf (stderr, "Too many redirects (%d)\n", MAX_REDIRECTS);
	else
	    fprintf (stderr, "HTTP initialization failed\n");
	net_free_url (url);
	free (loc);
	return NULL;
    }
    free (loc);

    /* Temp file extension comes from the final URL, post-redirects. */
    len = strlen("/tmp/vp.XXXXXX.")+strlen(url->ext)+1;
    filename = (char *)malloc (len);
    if (filename == NULL) {
	net_free_url (url);
	return NULL;
    }
    snprintf (filename, len, "/tmp/vp.XXXXXX.%s", url->ext);
    url->file = mkstemps (filename, strlen (url->ext) + 1);
    if (url->file == -1) {
	perror ("mkstemps failed");
	free (filename);
	net_free_url (url);
	return NULL;
    }

    if (net_suck (url) == -1) {
	fprintf (stderr, "Download failed\n");
	unlink (filename);  /* Remove partial file */
	free (filename);
	net_free_url (url);
	return NULL;
    }

    net_free_url (url);
    return filename;
}

void
net_purge (char *file)
{
    unlink (file);
    return;
}
