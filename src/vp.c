
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
#include <sys/types.h>
#include <sys/stat.h>

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
toggle_state (name)
{
    return (state ^= name);
}

void *
get_imglist ()
{
    return (void *)imglist;
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
    int imgcount = 0, i, count, c, wait = 2500;
    SDL_SysWMinfo info;
    Display *disp = NULL;

    static struct option optlist[] = {
	{"fullscreen", 0, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{"loud", 0, NULL, 'l'},
	{"sleep", 1, NULL, 's'},
	{"version", 0, NULL, 'v'},
	{"zoom", 0, NULL, 'z'},
	{0, 0, 0, 0}
    };

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
	    exit (printf
		("%s %s (C) 2001 Erik Greenwald <erik@smluc.org>\n",
		    PACKAGE, VERSION));
	    break;
	case 'z':
	    set_state_int (ZOOM);
	    break;
	}
    }

    image_table.image = malloc (sizeof (struct image_s) * (argc - optind));
    memset (image_table.image, 0, sizeof (struct image_table_s));
    memset (image_table.image, 0, sizeof (struct image_s) * (argc - optind));
    for (count = optind; count < argc; count++)
    {
	struct stat sb[1];

	if (stat (argv[count], sb) != -1 && sb->st_mode & S_IFREG)
	{
	    image_table.image[image_table.count].resource = argv[count];
	    image_table.count++;
	}
    }

    if (image_table.count == 0)
	oops ("No images selected... aborting.\n");

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);
    atexit (SDL_Quit);
    disp = XOpenDisplay (NULL);

    if (disp)
    {
	swidth = DisplayWidth (disp, DefaultScreen (disp));
	sheight = DisplayHeight (disp, DefaultScreen (disp));
	sdepth = BitmapUnit (disp);
    }
    if (get_state_int (FULLSCREEN))
	screen =
	    SDL_SetVideoMode (swidth, sheight, sdepth,
	    SDL_FULLSCREEN | SDL_DOUBLEBUF);
    else
	screen = SDL_SetVideoMode (1, 1, 32, SDL_DOUBLEBUF);

    SDL_ShowCursor (0);
    show_image ();

    if (image_table.count > 1)
	timer_start (wait);

    while (handle_input ());

    SDL_Quit ();
    return 0;
}
