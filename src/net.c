
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
#include <unistd.h>
#include "ftp.h"
#include "http.h"
#include "net.h"

url_t url;

int
net_is_url (char *name)
{
	return !strcmp(name,"http://")||!strcmp(name,"ftp://");
}

void
net_url(char *name)
{
	url.server=NULL;
	url.port=0;
	url.file=NULL;
	return;
}

char *
net_download (char *name)
{
	net_url(name);
    if (url.proto == HTTP)
	return http_download (url.server, url.port, url.file);
    else if (url.proto == FTP)
	return ftp_download (url.server, url.port, url.file);
    else
	return NULL;
}

void
net_purge (char *file)
{
    unlink (file);
    return;
}
