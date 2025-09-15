
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
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

#ifdef WIN32
# include <winsock.h>
# include <winsock2h>
#else
# include <sys/socket.h>
# include <sys/uio.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "ftp.h"
#include "http.h"
#include "net.h"

	/*
	 * FIXME 
	 */
#ifndef HAVE_MKSTEMPS

char
randchar ()
{
    switch (rand () % 3)
    {
    case 0:
	return rand () % 10 + '0';  /* Digits 0-9 */
	break;
    case 1:
	return rand () % 26 + 'A';  /* Letters A-Z */
	break;
    case 2:
	return rand () % 26 + 'a';  /* Letters a-z */
	break;
    }
    return 'X';
}

int
mkstemps (char *template, int suffixlen)
{
    int f;
    char *s;

    s = template;
    srand (getpid ());
    while (*s) {
	if (*s == 'X')
	    *s = randchar ();
	s++;
    }
    f = open (template, O_WRONLY | O_CREAT, 0600);
    return f;
}

#endif

int
net_is_url (char *name)
{
    return !strncmp (name, "http://", 7) || !strncmp (name, "ftp://", 6);
}

void
net_free_url (url_t *u)
{
    if (u) {
	if (u->server) free (u->server);
	if (u->filename) free (u->filename);
	if (u->ext) free (u->ext);
	if (u->mimetype) free (u->mimetype);
	if (u->file >= 0) close (u->file);
	if (u->conn >= 0) close (u->conn);
	free (u);
    }
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

    n = name;
    n += strlen ("http://");

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
	return NULL;
    }
    u->server = strdup (name + strlen ("http://"));
    u->filename = strdup (n);

    /* Fix buffer overflow - check filename length before accessing extension */
    int filename_len = strlen (n);
    if (filename_len >= 3) {
	u->ext = strdup (n + filename_len - 3);
    } else {
	u->ext = strdup ("");
    }

    /* Check for strdup failures */
    if (!u->server || !u->filename || !u->ext) {
	net_free_url (u);
	return NULL;
    }

    u->port = 80;
    u->proto = HTTP;
    u->mimetype = NULL;
    u->file = -1;
    u->conn = -1;
    return u;
}

int
net_connect (url_t * u)
{
    struct sockaddr_in s;
    struct sockaddr *ss = (struct sockaddr *)&s;
    struct hostent *h;

    memset (&s, 0, sizeof (s));
    if ((u->conn = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
	perror ("vp:net.c:net_connect:socket");
	return -1;
    }
    if ((h = gethostbyname (u->server)) == NULL)
    {
	perror ("vp:net.c:net_connect:gethostbyname");
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
    return 0;
}

int
net_suck (url_t * u)
{
    char buf[BUFSIZ];
    int len = BUFSIZ;
    size_t total_bytes = 0;
    const size_t max_download_size = 100 * 1024 * 1024;  /* 100MB limit */

    do
    {
	len = read (u->conn, buf, BUFSIZ);	/* TODO this stalls on the last packet */
	if (len < 0) {
	    /* Read error */
	    return -1;
	}
	if (len > 0) {
	    /* Check download size limit */
	    total_bytes += len;
	    if (total_bytes > max_download_size) {
		fprintf (stderr, "Download size limit exceeded (%zu bytes)\n", max_download_size);
		return -1;
	    }

	    if (write (u->file, buf, len) != len) {
		/* Write error */
		return -1;
	    }
	}
    }
    while (len > 0);
    return 0;
}

char *
net_download (char *name)
{
    char *filename;
    int len;
    url_t *url;

    if ((url = net_url (name)) == NULL || net_connect (url) == -1) {
	if (url) net_free_url (url);
	return NULL;
    }

    len = strlen("/tmp/vp.XXXX.")+strlen(url->ext)+1;
    filename = (char *)malloc (len);
    if (filename == NULL) {
	net_free_url (url);
	return NULL;
    }
    snprintf (filename, len, "/tmp/vp.XXXX.%s", url->ext);
    url->file = mkstemps (filename, strlen (url->ext) + 1);
    if (url->file == -1) {
	perror ("mkstemps failed");
	free (filename);
	net_free_url (url);
	return NULL;
    }

    switch (url->proto)
    {
    case HTTP:
	if (http_init (url) == -1) {
	    fprintf (stderr, "HTTP initialization failed\n");
	    unlink (filename);  /* Remove partial file */
	    free (filename);
	    net_free_url (url);
	    return NULL;
	}
	break;
    case FTP:
	if (ftp_init (url) == -1) {
	    fprintf (stderr, "FTP initialization failed\n");
	    unlink (filename);  /* Remove partial file */
	    free (filename);
	    net_free_url (url);
	    return NULL;
	}
	break;
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
