
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
#include <unistd.h>

#include "http.h"

int
http_init (url_t * u)
{
    char *buf;
    int e = 0;

    buf = (char *)malloc (BUFSIZ);
    if (buf == NULL) {
	return -1;
    }
    snprintf (buf, BUFSIZ, "\
GET /%s HTTP/1.1\n\
Host: %s\n\
Agent: %s %s\n\
\n", u->filename, u->server, PACKAGE, VERSION);
    write (u->conn, buf, strlen (buf));

    /*
     * Read HTTP headers with safety limits to prevent infinite loops
     */
    int bytes_read = 0;
    int max_header_size = 8192;  /* 8KB header limit */

    while (e < 4 && bytes_read < max_header_size)
    {
	ssize_t result = read (u->conn, buf, 1);
	if (result <= 0) {
	    /* Connection error or EOF */
	    free (buf);
	    return -1;
	}
	bytes_read++;

	if (*buf == '\n' || *buf == '\r')
	    e++;
	else
	    e = 0;
    }

    if (e < 4) {
	/* Headers too large or malformed */
	free (buf);
	return -1;
    }
    free (buf);
    return 0;
}
