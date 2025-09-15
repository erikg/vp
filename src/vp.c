
/*****************************************************************************
 * vp   -    SDL based image viewer for linux and fbsd. (X and console)      *
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>

#include "config.h"

#include "input.h"
#include "image.h"
#include "vp.h"
#include "timer.h"
#include "net.h"

#include "getopt.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_mutex *mutex;
static int state = 0;
int swidth = 640, sheight = 480, sdepth = 8;
struct image_table_s image_table = {0, 0, NULL};

unsigned int
vid_width ()
{
    int w;
    if (window) {
	SDL_GetWindowSize(window, &w, NULL);
	return w;
    }
    return swidth;
}
unsigned int
vid_height ()
{
    int h;
    if (window) {
	SDL_GetWindowSize(window, NULL, &h);
	return h;
    }
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
    int result;
    SDL_LockMutex (mutex);
    result = state & name;
    SDL_UnlockMutex (mutex);
    return result;
}

int
set_state_int (int name)
{
    int result;
    SDL_LockMutex (mutex);
    result = (state |= name);
    SDL_UnlockMutex (mutex);
    return result;
}

int
unset_state_int (int name)
{
    int result;
    SDL_LockMutex (mutex);
    result = (state &= ~name);
    SDL_UnlockMutex (mutex);
    return result;
}

int
toggle_state (int name)
{
    int result;
    SDL_LockMutex (mutex);
    result = (state ^= name);
    SDL_UnlockMutex (mutex);
    return result;
}

struct image_table_s *
get_image_table ()
{
    return &image_table;
}

/* Safe integer parsing with overflow detection */
static int
safe_atoi (const char *str, int *result, int min_val, int max_val)
{
    char *endptr;
    long val;

    if (!str || !result) {
        return -1;
    }

    errno = 0;
    val = strtol(str, &endptr, 10);

    /* Check for conversion errors */
    if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
        return -1;  /* Overflow */
    }

    /* Check for invalid characters */
    if (endptr == str || *endptr != '\0') {
        return -1;  /* No digits found or trailing garbage */
    }

    /* Check application-specific bounds */
    if (val < min_val || val > max_val) {
        return -1;  /* Out of acceptable range */
    }

    *result = (int)val;
    return 0;  /* Success */
}

/* Global flag for graceful shutdown */
static volatile sig_atomic_t shutdown_requested = 0;

/* Signal handler for graceful shutdown */
static void
signal_handler (int sig)
{
    shutdown_requested = 1;
    /* Send a quit event to the main loop */
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
}

void
oops (char *msg)
{
    fprintf (stderr, "%s\n", msg);
    if (mutex) {
	SDL_DestroyMutex (mutex);
    }
    SDL_Quit ();
    exit (EXIT_FAILURE);
}

void
show_help (char *name)
{
    printf ("Usage:\n\
\t%s [-fhlvz] [-s <seconds>] [-r [<width>][x<height>][@<depth>]]\n\
\n\
\t-f		--fullscreen	set fullscreen mode.\n\
\t-l		--loud		print file name to stdout.\n\
\t-h		--help		show help.\n\
\t-v		--version	show version.\n\
\t-z		--zoom		scale to fit when fullscreen.\n\
\t-s <seconds>	--sleep		seconds between image change in slideshow.\n\
\t-r <res>	--resolution	width, height, and depth. See man page.\n\
\n", name);
    return;
}

int
main (int argc, char **argv)
{
    int i, count, c, wait = 2500, width = 0, height = 0, depth = 0;

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
	    if (safe_atoi (optarg, &wait, 100, 60000) != 0) {
		fprintf (stderr, "Invalid sleep time: %s (must be 100-60000ms)\n", optarg);
		exit (EXIT_FAILURE);
	    }
	    break;
	case 'v':
	    exit (printf
		("%s %s (C) 2001-2017 Erik Greenwald <erik@elfga.com>\n",
		    PACKAGE, VERSION));
	    break;
	case 'z':
	    set_state_int (ZOOM);
	    break;
	case 'r':
	    {
		char *p, *x_pos, *at_pos;

		if (!optarg || strlen(optarg) == 0) {
		    fprintf (stderr, "Resolution cannot be empty\n");
		    exit (EXIT_FAILURE);
		}

		p = optarg;
		x_pos = strchr(p, 'x');
		at_pos = strchr(p, '@');

		/* Validate format - must have at least width and height */
		if (!x_pos) {
		    fprintf (stderr, "Invalid resolution format: %s (expected WIDTHxHEIGHT[@DEPTH])\n", optarg);
		    exit (EXIT_FAILURE);
		}

		/* Parse width */
		if (isdigit (*p)) {
		    char width_str[16];
		    int len = (int)(x_pos - p);
		    if (len <= 0 || len >= sizeof(width_str)) {
			fprintf (stderr, "Invalid resolution format: %s\n", optarg);
			exit (EXIT_FAILURE);
		    }
		    strncpy(width_str, p, len);
		    width_str[len] = '\0';
		    if (safe_atoi (width_str, &width, 64, 16384) != 0) {
			fprintf (stderr, "Invalid width: %s (must be 64-16384)\n", width_str);
			exit (EXIT_FAILURE);
		    }
		} else {
		    fprintf (stderr, "Width must start with a digit: %s\n", optarg);
		    exit (EXIT_FAILURE);
		}

		/* Parse height */
		{
		    char height_str[16];
		    char *h_start = x_pos + 1;
		    int len = at_pos ? (int)(at_pos - h_start) : strlen(h_start);
		    if (len <= 0 || len >= sizeof(height_str)) {
			fprintf (stderr, "Invalid resolution format: %s\n", optarg);
			exit (EXIT_FAILURE);
		    }
		    if (!isdigit (*h_start)) {
			fprintf (stderr, "Height must start with a digit: %s\n", optarg);
			exit (EXIT_FAILURE);
		    }
		    strncpy(height_str, h_start, len);
		    height_str[len] = '\0';
		    if (safe_atoi (height_str, &height, 64, 16384) != 0) {
			fprintf (stderr, "Invalid height: %s (must be 64-16384)\n", height_str);
			exit (EXIT_FAILURE);
		    }
		}

		/* Parse depth (optional) */
		if (at_pos) {
		    if (!isdigit (*(at_pos + 1))) {
			fprintf (stderr, "Depth must be a number: %s\n", at_pos + 1);
			exit (EXIT_FAILURE);
		    }
		    if (safe_atoi (at_pos + 1, &depth, 8, 32) != 0) {
			fprintf (stderr, "Invalid depth: %s (must be 8, 16, 24, or 32)\n", at_pos + 1);
			exit (EXIT_FAILURE);
		    }
		    /* Validate common depths */
		    if (depth != 8 && depth != 16 && depth != 24 && depth != 32) {
			fprintf (stderr, "Invalid depth: %d (must be 8, 16, 24, or 32)\n", depth);
			exit (EXIT_FAILURE);
		    }
		}

		/* Validate aspect ratio */
		double aspect_ratio = (double)width / (double)height;
		if (aspect_ratio < 0.1 || aspect_ratio > 10.0) {
		    fprintf (stderr, "Invalid aspect ratio: %dx%d (ratio %.2f is unrealistic)\n",
			     width, height, aspect_ratio);
		    exit (EXIT_FAILURE);
		}
	    }
	    break;
	case 'h':
	default:
	    show_help (argv[0]);
	    return 0;
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Check for integer overflow in allocation size */
    if (argc > SIZE_MAX / sizeof(struct image_s)) {
	fprintf (stderr, "Too many arguments\n");
	exit (EXIT_FAILURE);
    }

    image_table.image = malloc (sizeof (struct image_s) * argc);
    if (image_table.image == NULL) {
	fprintf (stderr, "Out of memory\n");
	exit (EXIT_FAILURE);
    }
    memset (image_table.image, 0, sizeof (struct image_s) * argc);

    printf ("Scanning for images, %d possible\n", argc);

    for (count = 0; count < argc; count++)
    {
	struct stat sb[1];

	if (stat (argv[count], sb) != -1 && !(sb->st_mode & S_IFDIR))
	{
	    /* Check bounds before accessing array */
	    if (image_table.count >= argc) {
		fprintf (stderr, "Internal error: image_table overflow\n");
		exit (EXIT_FAILURE);
	    }
	    image_table.image[image_table.count].resource = argv[count];
	    image_table.image[image_table.count].file = argv[count];
	    image_table.count++;
	} else if(net_is_url(argv[count])) {
	    char *downloaded_file = net_download(argv[count]);
	    if (downloaded_file) {
		/* Check bounds before accessing array */
		if (image_table.count >= argc) {
		    fprintf (stderr, "Internal error: image_table overflow\n");
		    free (downloaded_file);
		    exit (EXIT_FAILURE);
		}
		image_table.image[image_table.count].resource = argv[count];
		image_table.image[image_table.count].file = downloaded_file;
		image_table.count++;
	    }
	}
    }

    if (image_table.count == 0)
	oops ("No images selected... aborting.\n");

    SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER);
    atexit (SDL_Quit);
    mutex = SDL_CreateMutex ();

    /* Install signal handlers for graceful shutdown */
    signal (SIGINT, signal_handler);   /* Ctrl+C */
    signal (SIGTERM, signal_handler);  /* Termination request */
    #ifdef SIGHUP
    signal (SIGHUP, signal_handler);   /* Hangup */
    #endif

    /* Get desktop display mode for fullscreen */
    SDL_DisplayMode desktop_mode;
    if (SDL_GetDesktopDisplayMode (0, &desktop_mode) == 0) {
	swidth = desktop_mode.w;
	sheight = desktop_mode.h;
	sdepth = SDL_BITSPERPIXEL (desktop_mode.format);
    }

    if (width)
	swidth = width;
    if (height)
	sheight = height;
    if (depth)
	sdepth = depth;

    /* Create window */
    Uint32 window_flags = 0;
    if (get_state_int (FULLSCREEN)) {
	window_flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	SDL_ShowCursor (0);
    }

    window = SDL_CreateWindow ("vp",
	SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	get_state_int (FULLSCREEN) ? swidth : 1,
	get_state_int (FULLSCREEN) ? sheight : 1,
	window_flags);

    if (window == NULL)
    {
	printf ("Unable to create window: %s\n", SDL_GetError ());
	return EXIT_FAILURE;
    }

    /* Create renderer */
    renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
	SDL_DestroyWindow (window);
	printf ("Unable to create renderer: %s\n", SDL_GetError ());
	return EXIT_FAILURE;
    }

    image_freshen ();

    if (image_table.count > 1)
	timer_start (wait);

    while (handle_input ());

    if (image_table.image) {
	/* Free downloaded filenames and clean up image surfaces */
	for (int i = 0; i < image_table.count; i++) {
	    if (image_table.image[i].surface) {
		SDL_FreeSurface (image_table.image[i].surface);
	    }
	    if (image_table.image[i].scaled) {
		SDL_FreeSurface (image_table.image[i].scaled);
	    }
	    /* Free downloaded filenames (they differ from resource) */
	    if (image_table.image[i].file != image_table.image[i].resource) {
		free (image_table.image[i].file);
	    }
	}
	free (image_table.image);
    }
    if (renderer) {
	SDL_DestroyRenderer (renderer);
    }
    if (window) {
	SDL_DestroyWindow (window);
    }
    if (mutex) {
	SDL_DestroyMutex (mutex);
    }
    SDL_Quit ();
    return 0;
}
