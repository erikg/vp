
/*****************************************************************************
 * vp   -    SDL based image viewer for linux and fbsd. (X and console)      *
 * Copyright (C) 2001-2004 Erik Greenwald <erik@smluc.org>                   *
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

/*
 * $Id: vp.c,v 1.19 2005/01/10 15:47:26 erik Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>

#include "config.h"

#include "input.h"
#include "image.h"
#include "vp.h"
#include "timer.h"

#include "getopt.h"

SDL_Surface *screen;
SDL_mutex *mutex;
static int state;
int swidth = 640, sheight = 480, sdepth = 8;
struct image_table_s image_table;

unsigned int
vid_width ()
{
    return swidth;
}
unsigned int
vid_height ()
{
    return sheight;
}
unsigned int
vid_depth ()
{
    return sdepth;
}

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
toggle_state (int name)
{
    return (state ^= name);
}

struct image_table_s *
get_image_table ()
{
    return &image_table;
}

void
oops (char *msg)
{
    fprintf (stderr, "%s\n", msg);
    SDL_Quit ();
    exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
    int i, count, c, wait = 2500, width = 0, height = 0, depth = 0, haveres =
	0;

/*
    SDL_SysWMinfo info;
*/
#ifdef SDL_SYSWM_X11
    Display *disp = NULL;
#endif

    static struct option optlist[] = {
	{"fullscreen", 0, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{"loud", 0, NULL, 'l'},
	{"sleep", 1, NULL, 's'},
	{"version", 0, NULL, 'v'},
	{"zoom", 0, NULL, 'z'},
	{"resolution", 1, NULL, 'r'},
	{0, 0, 0, 0}
    };

    while ((c = getopt_long (argc, argv, "vhlzfs:r:", optlist, &i)) != -1)
    {
	switch (c)
	{
	case 'f':
	    set_state_int (FULLSCREEN);
	    break;
	case 'l':
	    set_state_int (LOUD);
	    break;
	case 's':
	    wait = atoi (optarg);
	    break;
	case 'v':
	    exit (printf
		("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
		    PACKAGE, VERSION));
	    break;
	case 'z':
	    set_state_int (ZOOM);
	    break;
	case 'r':
	    {
		char *w = NULL, *h = NULL, *d = NULL, *p;

		p = optarg;
		w = p;
		while (*p)
		{
		    if (*p == 'x')
			h = p + 1;
		    if (*p == '@')
			d = p + 1;
		    ++p;
		}
		width = atoi (w);
		height = atoi (h);
		if (d)
		    depth = atoi (d);
		haveres = 1;
	    }
	    break;
	case 'h':
	default:
	    /*
	     * show help 
	     */
	    return 0;
	    break;
	}
    }

    image_table.image = malloc (sizeof (struct image_s) * (argc - optind));
    memset (image_table.image, 0, sizeof (struct image_s) * (argc - optind));
    printf("Scanning for images, %d possible\n", argc);
    for (count = optind; count < argc; count++)
    {
	struct stat sb[1];

	if (stat (argv[count], sb) != -1 && !(sb->st_mode & S_IFDIR))
	{
	    image_table.image[image_table.count].resource = argv[count];
	    image_table.count++;
	}
    }

    if (image_table.count == 0)
	oops ("No images selected... aborting.\n");

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);
    atexit (SDL_Quit);
    mutex = SDL_CreateMutex ();

#ifdef SDL_SYSWM_X11
    disp = XOpenDisplay (NULL);

    if (disp)
    {
	swidth = DisplayWidth (disp, DefaultScreen (disp));
	sheight = DisplayHeight (disp, DefaultScreen (disp));
	sdepth = BitmapUnit (disp);
    }
#endif

    if (width)
	swidth = width;
    if (height)
	sheight = height;
    if (depth)
	sdepth = depth;

    if (get_state_int (FULLSCREEN))
    {
	printf ("Fullscreen!\n");
	printf ("%dx%d@%d\n", swidth, sheight, sdepth);
	screen =
	    SDL_SetVideoMode (swidth, sheight, sdepth,
	    SDL_FULLSCREEN | SDL_DOUBLEBUF);
	printf ("%s\n", SDL_GetError ());
    } else
	screen = SDL_SetVideoMode (1, 1, 32, SDL_DOUBLEBUF);
    if (screen == NULL)
    {
	printf ("Unable to grab screen\n");
	return EXIT_FAILURE;
    }

    SDL_ShowCursor (0);
    image_freshen ();

    if (image_table.count > 1)
	timer_start (wait);

    while (handle_input ());

    SDL_Quit ();
    return 0;
}
