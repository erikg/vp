
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

#include <SDL.h>
#include <SDL_image.h>

#include "input.h"
#include "image.h"
#include "iview.h"
#include "ll.h"
#include "timer.h"

SDL_Surface *screen;
static void *imglist;		/* linked list */
static int state;

int
get_state_int (int name)
{
    return state & name;
}

int
set_state_int (int name)
{
    return (state |= name);
}

int
unset_state_int (int name)
{
    return (state &= ~name);
}

int
toggle_state (name)
{
    return (state ^= name);
}

void *
get_imglist ()
{
    return imglist;
}

void
oops (char *msg)
{
    fprintf (stderr, "%s\n", msg);
    SDL_Quit ();
    exit (-1);
}

void
parse_control_block (char *word)
{
    if (!strcmp (word, "-z") || !strcmp (word, "--zoom"))
	set_state_int (ZOOM);
    else if (!strcmp (word, "-l") || !strcmp (word, "--loud"))
	set_state_int (LOUD);
    else if (!strcmp (word, "-f") || !strcmp (word, "--fullscreen"))
	set_state_int (FULLSCREEN);
    else if (!strcmp (word, "-v") || !strcmp (word, "--version"))
	exit (printf ("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
		      PACKAGE, VERSION));
    return;
}

int
main (int argc, char **argv)
{
    int x, imgcount = 0;
    int count;

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);

    x = SDL_DOUBLEBUF;
    unset_state_int (FULLSCREEN);
    imglist = ll_newlist ();
    for (count = 1; count < argc; count++)
    {
	if (argv[count][0] == '-')
	    parse_control_block (argv[count]);
	else
	{
	    ll_addatend (imglist, argv[count]);
	    imgcount++;
	}
    }
    if (imgcount == 0)
	return 0;
    x |= get_state_int (FULLSCREEN);
    screen = SDL_SetVideoMode (1280, 1024, 32, x);

    SDL_ShowCursor (0);

    ll_rewind (imglist);
    image_init ();

    img_freshen();

	show_image ();
    if (imgcount >= 2)
	timer_start ();

    while (handle_input ());

    SDL_Quit ();
    return 0;
}
