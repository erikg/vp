
/*****************************************************************************
 * siview    -    SDL based image viewer for linux and fbsd. (X and console)  *
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
#include <unistd.h>
#include "http.h"

int
http_init (url_t * u)
{
    char *buf;
    int e = 0;

    buf = (char *)malloc (BUFSIZ);
    sprintf (buf, "GET /%s HTTP/1.1\nHost: %s\n\n\n", u->filename, u->server);
    write (u->conn, buf, strlen (buf));

    /*
     * FIXME this is ugly 
     */
    while (e < 4)
    {
	read (u->conn, buf, 1);
	if (*buf == '\n' || *buf == '\r')
	    e++;
	else
	    e = 0;
    }
    free (buf);
    return 0;
}
