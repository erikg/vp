
/*****************************************************************************
 * vp   -    SDL based image viewer for linux and fbsd. (X and console)  *
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
#include <SDL_syswm.h>

#include "input.h"
#include "image.h"
#include "vp.h"
#include "ll.h"
#include "timer.h"

#include "getopt.h"

SDL_Surface *screen;
static void *imglist;		/* linked list */
static int state;
int swidth, sheight, sdepth;

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

int
main (int argc, char **argv)
{
    int x, imgcount = 0, i, count, c, wait = 2500;
    SDL_SysWMinfo info;

    static struct option optlist[] = {
	{"fullscreen", 0, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{"loud", 0, NULL, 'l'},
	{"sleep", 1, NULL, 's'},
	{"version", 0, NULL, 'v'},
	{"zoom", 0, NULL, 'z'},
	{0, 0, 0, 0}
    };

    x = SDL_DOUBLEBUF;
    unset_state_int (FULLSCREEN);
    unset_state_int (GRAB_FOCUS);
    imglist = ll_newlist ();

    while ((c = getopt_long (argc, argv, "vhlzfs:", optlist, &i)) != -1)
    {
	switch (c)
	{
	case 'f':
	    set_state_int (FULLSCREEN);
	    break;
	case 'h':
	    break;
	case 'l':
	    set_state_int (LOUD);
	    break;
	case 's':
	    wait = atoi (optarg);
	    break;
	case 'v':
	    exit (printf ("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
		    PACKAGE, VERSION));
	    break;
	case 'z':
	    set_state_int (ZOOM);
	    break;
	}
    }

    for (count = optind; count < argc; count++)
    {
	ll_addatend (imglist, argv[count]);
	imgcount++;
    }

    ll_rewind (imglist);
    if (imgcount == 0 || image_init () != 0)
    {
	printf ("No images selected... aborting.\n");
	return 0;
    }
    x |= get_state_int (FULLSCREEN);

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);
    atexit (SDL_Quit);		/* as much as I hate doing this, it's necessary.
				 * libjpeg seems to like to exit() on bad image,
				 * instead of doing the right thing and returning an
				 * error code. :/  */
    if (get_state_int (FULLSCREEN))
    {
		/* this fails, why? */
	if (SDL_GetWMInfo (&info) > 0 /*&& info.subsystem==SDL_SYSWM_X11 */ )
	{
	    swidth = DisplayWidth (info.info.x11.display,
		DefaultScreen (info.info.x11.display));
	    sheight = DisplayHeight (info.info.x11.display,
		DefaultScreen (info.info.x11.display));
	    sdepth = BitmapUnit (info.info.x11.display);
	    printf ("display: %dx%d@%d\n", swidth, sheight, sdepth);
	}
	swidth = 1280;
	sheight = 1024;
	sdepth = 24;
	screen = SDL_SetVideoMode (swidth, sheight, sdepth, x);
    } else
	screen = SDL_SetVideoMode (10, 10, 32, 0);

    SDL_ShowCursor (0);
    show_image ();

    if (imgcount >= 2)
	timer_start (wait);

	/* this is a message loop, it should be signalled or interrupted, not spin-polled */
    while (handle_input ())
	SDL_Delay(1);	/* surrender to the kernel */

    SDL_Quit ();
    return 0;
}
