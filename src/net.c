
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
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "ftp.h"
#include "http.h"
#include "net.h"

	/*
	 * ick 
	 */
char *filename;

int
net_is_url (char *name)
{
    return !strcmp (name, "http://") || !strcmp (name, "ftp://");
}

url_t *
net_url (char *name)
{
    url_t *u;

    u = (url_t *) malloc (sizeof (url_t));
    u->server = NULL;
    u->port = 0;
    u->file = NULL;
    return u;
}

int
net_connect (url_t * u)
{
    return 0;
}

int
net_suck(url_t *u)
{
	char buf[BUFSIZ];
	int len;
	while(len=read(u->conn,buf,BUFSIZ))
		if(write(u->conn,buf,len)!=len)
			return -1;
	return 0;
}

char *
net_download (char *name)
{
    int socket, file;
    url_t *url;

    if ((url = net_url (name)) == NULL || net_connect (url) == 0)
	return NULL;
    switch (url->proto)
    {
    case HTTP:
	http_init (url);
	break;
    case FTP:
	ftp_init (url);
	break;
    }
    filename =
	(char *) malloc (strlen ("iview.XXXX.") + strlen (url->ext) + 1);
    sprintf (filename, "iview.XXXX.%s", url->ext);
    url->file = mkstemps (filename, strlen (url->ext) + 1);
    if(net_suck (url)==-1)
		printf("Some problem reading file (suck blew)...\n");
	close(url->conn);
	close(url->file);
    free (url);
    return filename;
}

void
net_purge (char *file)
{
    unlink (file);
    return;
}
