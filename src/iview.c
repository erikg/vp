
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

#include <stdlib.h>
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
    if (word[0] == '-')
    {
	/*
	 * long options 
	 */
	word++;
	if (!strcmp (word , "version"))
	    exit (printf
		("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
		    PACKAGE, VERSION));

	if (!strcmp (word, "fullscreen"))
	    set_state_int (FULLSCREEN);
	if (!strcmp (word, "loud"))
	    set_state_int (LOUD);
	if (!strcmp (word, "zoom"))
	    set_state_int (ZOOM);
    } else
	while (word[0] != 0)
	{
	    switch (word[0])
	    {
	    case 'z':
	    case 'Z':
		set_state_int (ZOOM);
		break;
	    case 'l':
	    case 'L':
		set_state_int (LOUD);
		break;
	    case 'f':
	    case 'F':
		set_state_int (FULLSCREEN);
		break;
	    case 'v':
	    case 'V':
		exit (printf
		    ("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
			PACKAGE, VERSION));
		break;
	    default:
		printf ("Unknown command\n");
	    }
	    word++;
	}
    return;
}

int
main (int argc, char **argv)
{
    int x, imgcount = 0;
    int count;

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);
    atexit (SDL_Quit);		/* as much as I hate doing this, it's necessary.
				 * libjpeg seems to like to exit() on bad image,
				 * instead of doing the right thing and returning an
				 * error code. :/  */

    x = SDL_DOUBLEBUF;
    unset_state_int (FULLSCREEN);
    set_state_int (GRAB_FOCUS);
    imglist = ll_newlist ();

    /*
     * this should probably use getopt, or at least a more unix like
     * method... 
     */
    for (count = 1; count < argc; count++)
    {
	if (argv[count][0] == '-')
	    parse_control_block (argv[count] + 1);
	else
	{
	    ll_addatend (imglist, argv[count]);
	    imgcount++;
	}
    }
    ll_rewind (imglist);
    if (imgcount == 0 || image_init () != 0)
    {
	printf ("No images selected... aborting.\n");
	return 0;
    }
    x |= get_state_int (FULLSCREEN);

    if (x & FULLSCREEN)
	screen = SDL_SetVideoMode (1280, 1024, 32, x);
    SDL_ShowCursor (0);

    show_image ();
    if (imgcount >= 2)
	timer_start (2500);

    while (handle_input ());

    SDL_Quit ();
    return 0;
}
