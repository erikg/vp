
/*****************************************************************************
 * iview    -    SDL based image viewer for linux and fbsd. (X and console)  *
 * Copyright (C) 2001 Erik Greenwald <erik@smluc.org>                        *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ftp.h"
#include "http.h"
#include "net.h"

	/*
	 * ick 
	 */
char *filename;

#ifndef mkstemps
int mkstemps(char *template, int suffixlen)
{
	printf("mkstemps says.. \'%s\' with %d\n",
		template, suffixlen);
	return 0;
}
#endif

int
net_is_url (char *name)
{
    return !strncmp (name, "http://", 7) || !strncmp (name, "ftp://", 7);
}

url_t *
net_url (char *name)
{
    url_t *u;
    char *n;

    n = name;
    printf ("%s\n", n);
    n += strlen ("http://") + 1;
    while (*n != '/')
	n++;
    *n = 0;
    n++;
    u = (url_t *) malloc (sizeof (url_t));
    u->server = strdup (name + strlen ("http://"));
    u->port = 80;
    u->filename = strdup (n);
    u->ext = strdup (n + strlen (n) - 3);
    u->proto = HTTP;
    printf ("getting %s (%s) from %s\n", u->filename, u->ext, u->server);
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
	perror ("iview:net.c:net_connect:socket");
	return -1;
    }
    if ((h = gethostbyname (u->server)) == NULL)
    {
	perror ("iview:net.c:net_connect:gethostbyname");
	return -1;
    }
    s.sin_family = AF_INET;
    s.sin_port = htons (u->port);
    s.sin_addr = *((struct in_addr *)h->h_addr_list[0]);
    if (connect (u->conn, ss, sizeof (struct sockaddr)) == -1)
    {
	perror ("iview:net.c:net_connect:connect");
	return -1;
    }
    return 0;
}

int
net_suck (url_t * u)
{
    char buf[BUFSIZ];
    int len = BUFSIZ;

    do
    {
	len = read (u->conn, buf, BUFSIZ);
	if (write (u->file, buf, len) != len)
	    return -1;
    }

    while (len /*== BUFSIZ*/ );
    return 0;
}

char *
net_download (char *name)
{

/*
    int socket, file;
*/
    url_t *url;

    printf ("Download %s\n", name);
    if ((url = net_url (name)) == NULL || net_connect (url) == -1)
	return NULL;
    filename =
	(char *)malloc (strlen ("/tmp/iview.XXXX.") + strlen (url->ext) + 1);
    sprintf (filename, "/tmp/iview.XXXX.%s", url->ext);
    url->file = mkstemps (filename, strlen (url->ext) + 1);
    printf ("saving to %s\n", filename);
    switch (url->proto)
    {
    case HTTP:
	http_init (url);
	break;
    case FTP:
	ftp_init (url);
	break;
    }
    if (net_suck (url) == -1)
	printf ("Some problem reading file (suck blew)...\n");
    close (url->conn);
    close (url->file);
    free (url);
    return filename;
}

void
net_purge (char *file)
{
    unlink (file);
    return;
}
