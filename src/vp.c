
/*****************************************************************************
 * vp   -    SDL based image viewer for linux and fbsd. (X and console)      *
 * Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>                   *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "input.h"
#include "image.h"
#include "vp.h"
#include "timer.h"
#include "net.h"

#include <getopt.h>

SDL_Window *window;
SDL_Renderer *renderer;
SDL_mutex *mutex;
static int state = 0;
int swidth = 640, sheight = 480;

/* Which flavor of fullscreen 'f' toggles into: desktop fullscreen by
 * default; -r upgrades it to a real mode-switching fullscreen. */
Uint32 fullscreen_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
struct image_table_s image_table = {0, 0, NULL};

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
get_image_table (void)
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

/* Signal handler for graceful shutdown.
 * Async-signal-safe: only sets the flag. The main loop uses a timed wait
 * (see handle_input) so it polls this flag and exits promptly. */
static void
signal_handler (int sig)
{
    (void) sig;
    shutdown_requested = 1;
}

/*
 * Unlink downloaded temp files, free their name strings and any decoded
 * surfaces, and release the image table. Every exit path that runs after
 * the download loop must come through here or downloads are stranded in
 * /tmp. (Local files share the resource pointer and must be left alone.)
 * With -K the temp files are kept, and named so they can be found.
 */
static void
free_image_table (void)
{
    if (image_table.image == NULL)
	return;
    for (int n = 0; n < image_table.count; n++) {
	if (image_table.image[n].surface)
	    SDL_FreeSurface (image_table.image[n].surface);
	if (image_table.image[n].file != image_table.image[n].resource) {
	    if (get_state_int (KEEP))
		printf ("kept %s (%s)\n", image_table.image[n].file,
		    image_table.image[n].resource);
	    else
		net_purge (image_table.image[n].file);
	    free (image_table.image[n].file);
	}
    }
    free (image_table.image);
    image_table.image = NULL;
    image_table.count = 0;
}

void
oops (const char *msg)
{
    fprintf (stderr, "vp: %s\n", msg);
    free_image_table ();
    if (mutex) {
	SDL_DestroyMutex (mutex);
    }
    SDL_Quit ();
    exit (EXIT_FAILURE);
}

/*
 * Create the renderer, preferring hardware acceleration but falling back to
 * the software renderer so vp still runs on machines with no 3D at all
 * (old laptops, minimal-fbdev embedded boards, etc.). The VP_RENDERER
 * environment variable forces a mode for debugging/testing:
 *   auto (default) - try accelerated, fall back to software
 *   accelerated|hw - hardware only (fail if unavailable)
 *   software|sw    - software only
 */
static SDL_Renderer *
create_renderer (SDL_Window * w)
{
    const char *mode = getenv ("VP_RENDERER");
    SDL_Renderer *r = NULL;

    if (mode && (!strcasecmp (mode, "software") || !strcasecmp (mode, "sw")))
	return SDL_CreateRenderer (w, -1, SDL_RENDERER_SOFTWARE);

    if (mode && (!strcasecmp (mode, "accelerated") || !strcasecmp (mode, "hw")))
	return SDL_CreateRenderer (w, -1, SDL_RENDERER_ACCELERATED);

    if (mode && strcasecmp (mode, "auto") != 0)
	fprintf (stderr,
	    "vp: Unknown VP_RENDERER \"%s\" (use auto|accelerated|software); using auto\n",
	    mode);

    /* auto: best available, with software as the safety net */
    r = SDL_CreateRenderer (w, -1, SDL_RENDERER_ACCELERATED);
    if (r == NULL)
	r = SDL_CreateRenderer (w, -1, SDL_RENDERER_SOFTWARE);
    return r;
}

static void
show_help (char *name)
{
    printf ("Usage:\n\
\t%s [-fhkKlvz] [-s <seconds>] [-r <width>x<height>] file-or-url ...\n\
\n\
\t-f		--fullscreen	set fullscreen mode.\n\
\t-h		--help		show help.\n\
\t-k		--insecure	accept https certificates that fail\n\
\t				verification.\n\
\t-K		--keep		keep downloaded files instead of\n\
\t				deleting them on exit.\n\
\t-l		--loud		print file name to stdout.\n\
\t-s <seconds>	--sleep		seconds between image change in slideshow\n\
\t				(0.1-60, fractions ok, e.g. 2.5); default 2.5.\n\
\t-r <res>	--resolution	fullscreen resolution, <width>x<height>;\n\
\t				closest available mode is used.\n\
\t-v		--version	show version.\n\
\t-z		--zoom		scale images to fit the screen.\n\
\n", name);
    return;
}

int
main (int argc, char **argv)
{
    int i, count, c, wait = DEFAULT_SLIDESHOW_MS, width = 0, height = 0;

    /* The state accessors lock this, and getopt handlers below already use
     * them, so create it before anything else can touch it. (SDL mutexes
     * don't need SDL_Init.) */
    mutex = SDL_CreateMutex ();
    if (mutex == NULL)
	oops ("SDL_CreateMutex() failed");

    static struct option optlist[] = {
	{"fullscreen", 0, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{"insecure", 0, NULL, 'k'},
	{"keep", 0, NULL, 'K'},
	{"loud", 0, NULL, 'l'},
	{"sleep", 1, NULL, 's'},
	{"version", 0, NULL, 'v'},
	{"zoom", 0, NULL, 'z'},
	{"resolution", 1, NULL, 'r'},
	{0, 0, 0, 0}
    };

    while ((c = getopt_long (argc, argv, "vhkKlzfs:r:", optlist, &i)) != -1)
    {
	switch (c)
	{
	case 'f':
	    set_state_int (FULLSCREEN);
	    break;
	case 'k':
	    net_allow_bad_certs ();
	    break;
	case 'K':
	    set_state_int (KEEP);
	    break;
	case 'l':
	    set_state_int (LOUD);
	    break;
	case 's':
	    {
		char *end;
		double secs = strtod (optarg, &end);

		/* Seconds, fractions allowed (-s 2.5 == 2500ms). The
		 * !(a && b) form also rejects NaN. */
		if (end == optarg || *end != '\0' ||
		    !(secs >= 0.1 && secs <= 60.0)) {
		    fprintf (stderr,
			"vp: Invalid sleep time: %s (seconds, 0.1-60)\n", optarg);
		    exit (EXIT_FAILURE);
		}
		wait = (int) (secs * 1000.0 + 0.5);
	    }
	    break;
	case 'v':
	    printf ("%s %s (C) 2001-2026 Erik Greenwald <erik@elfga.com>\n",
		PACKAGE, VERSION);
	    exit (EXIT_SUCCESS);
	    break;
	case 'z':
	    set_state_int (ZOOM);
	    break;
	case 'r':
	    {
		char *p, *x_pos;

		if (!optarg || strlen(optarg) == 0) {
		    fprintf (stderr, "vp: Resolution cannot be empty\n");
		    exit (EXIT_FAILURE);
		}

		p = optarg;
		x_pos = strchr(p, 'x');

		/* @depth was an svgalib-era knob; SDL2 renderers have no
		 * notion of display depth, so reject it loudly rather than
		 * silently ignoring it. */
		if (strchr (p, '@')) {
		    fprintf (stderr, "vp: @depth is no longer supported: %s (expected WIDTHxHEIGHT)\n", optarg);
		    exit (EXIT_FAILURE);
		}

		/* Validate format - must have at least width and height */
		if (!x_pos) {
		    fprintf (stderr, "vp: Invalid resolution format: %s (expected WIDTHxHEIGHT)\n", optarg);
		    exit (EXIT_FAILURE);
		}

		/* Parse width. (ctype on a plain char is UB for high-bit
		 * bytes; cast keeps it defined.) */
		if (isdigit ((unsigned char) *p)) {
		    char width_str[16];
		    int len = (int)(x_pos - p);
		    if (len <= 0 || (size_t)len >= sizeof(width_str)) {
			fprintf (stderr, "vp: Invalid resolution format: %s\n", optarg);
			exit (EXIT_FAILURE);
		    }
		    strncpy(width_str, p, len);
		    width_str[len] = '\0';
		    if (safe_atoi (width_str, &width, 64, 16384) != 0) {
			fprintf (stderr, "vp: Invalid width: %s (must be 64-16384)\n", width_str);
			exit (EXIT_FAILURE);
		    }
		} else {
		    fprintf (stderr, "vp: Width must start with a digit: %s\n", optarg);
		    exit (EXIT_FAILURE);
		}

		/* Parse height: with @depth gone it runs to end of string,
		 * so no substring copy is needed. */
		{
		    char *h_start = x_pos + 1;

		    if (!isdigit ((unsigned char) *h_start)) {
			fprintf (stderr, "vp: Height must start with a digit: %s\n", optarg);
			exit (EXIT_FAILURE);
		    }
		    if (safe_atoi (h_start, &height, 64, 16384) != 0) {
			fprintf (stderr, "vp: Invalid height: %s (must be 64-16384)\n", h_start);
			exit (EXIT_FAILURE);
		    }
		}

		/* Validate aspect ratio */
		double aspect_ratio = (double)width / (double)height;
		if (aspect_ratio < 0.1 || aspect_ratio > 10.0) {
		    fprintf (stderr, "vp: Invalid aspect ratio: %dx%d (ratio %.2f is unrealistic)\n",
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
    if ((size_t)argc > SIZE_MAX / sizeof(struct image_s)) {
	fprintf (stderr, "vp: Too many arguments\n");
	exit (EXIT_FAILURE);
    }

    image_table.image = malloc (sizeof (struct image_s) * argc);
    if (image_table.image == NULL) {
	fprintf (stderr, "vp: Out of memory\n");
	exit (EXIT_FAILURE);
    }
    memset (image_table.image, 0, sizeof (struct image_s) * argc);

    printf ("Scanning for images, %d possible\n", argc);

    /* Install signal handlers before the download phase, not after: a
     * Ctrl+C during a slow fetch should still reach the cleanup path
     * below instead of stranding temp files via default disposition.
     * SIGPIPE is ignored outright - a server resetting the connection
     * mid-write must surface as a write error, not kill the viewer. */
    signal (SIGINT, signal_handler);   /* Ctrl+C */
    signal (SIGTERM, signal_handler);  /* Termination request */
#ifdef SIGHUP
    signal (SIGHUP, signal_handler);   /* Hangup */
#endif
#ifdef SIGPIPE
    signal (SIGPIPE, SIG_IGN);
#endif

    for (count = 0; count < argc && !shutdown_requested; count++)
    {
	struct stat sb[1];

	/* Regular files only: directories obviously, but also fifos (a
	 * writer-less fifo hangs IMG_Load) and device nodes. */
	if (stat (argv[count], sb) != -1 && S_ISREG (sb->st_mode))
	{
	    /* Check bounds before accessing array */
	    if (image_table.count >= argc) {
		fprintf (stderr, "vp: Internal error: image_table overflow\n");
		free_image_table ();
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
		    fprintf (stderr, "vp: Internal error: image_table overflow\n");
		    if (!get_state_int (KEEP))
			net_purge (downloaded_file);
		    free (downloaded_file);
		    free_image_table ();
		    exit (EXIT_FAILURE);
		}
		image_table.image[image_table.count].resource = argv[count];
		image_table.image[image_table.count].file = downloaded_file;
		image_table.count++;
	    } else
		fprintf (stderr, "vp: %s: fetch failed, skipping\n", argv[count]);
	} else
	    fprintf (stderr, "vp: %s: not a readable file, skipping\n", argv[count]);
    }

    if (shutdown_requested)
	oops ("Interrupted.");

    if (image_table.count == 0)
	oops ("No images selected... aborting.");

    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
	fprintf (stderr, "vp: Unable to initialize SDL: %s\n", SDL_GetError ());
	free_image_table ();
	return EXIT_FAILURE;
    }
    atexit (SDL_Quit);

    /* Get desktop display mode for fullscreen */
    SDL_DisplayMode desktop_mode;
    if (SDL_GetDesktopDisplayMode (0, &desktop_mode) == 0) {
	swidth = desktop_mode.w;
	sheight = desktop_mode.h;
    }

    /* -r asks for a real display mode: find the closest one the display
     * offers and switch fullscreen behavior from desktop-sized to
     * mode-setting. X11, the KMS/DRM console, and macOS switch the
     * actual mode; Wayland compositors emulate it by scaling. */
    SDL_DisplayMode fs_mode;
    if (width && height) {
	SDL_DisplayMode want;

	SDL_zero (want);
	want.w = width;
	want.h = height;
	if (SDL_GetClosestDisplayMode (0, &want, &fs_mode) != NULL) {
	    fullscreen_flag = SDL_WINDOW_FULLSCREEN;
	    swidth = fs_mode.w;
	    sheight = fs_mode.h;
	    if (fs_mode.w != width || fs_mode.h != height)
		fprintf (stderr, "vp: no %dx%d display mode, using %dx%d\n",
		    width, height, fs_mode.w, fs_mode.h);
	} else
	    fprintf (stderr,
		"vp: no display mode near %dx%d, using the desktop mode\n",
		width, height);
    }

    /* Create window */
    Uint32 window_flags = 0;
    if (get_state_int (FULLSCREEN)) {
	window_flags = fullscreen_flag;
	SDL_ShowCursor (0);
    }

    window = SDL_CreateWindow ("vp",
	SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	get_state_int (FULLSCREEN) ? swidth : 1,
	get_state_int (FULLSCREEN) ? sheight : 1,
	window_flags);

    if (window == NULL)
    {
	fprintf (stderr, "vp: Unable to create window: %s\n", SDL_GetError ());
	free_image_table ();
	return EXIT_FAILURE;
    }

    /* Pin the chosen mode to the window even when starting windowed, so a
     * later 'f' toggle into SDL_WINDOW_FULLSCREEN honors -r too. */
    if (fullscreen_flag == SDL_WINDOW_FULLSCREEN)
	SDL_SetWindowDisplayMode (window, &fs_mode);

    /* A terminal-launched process is not the active app on macOS, and
     * cooperative activation (macOS 14+) leaves the new window behind
     * the terminal it was launched from. Cocoa ignores a raise before
     * the window is actually on screen, and may drop the first request
     * outright, so pump and re-ask until focus lands (or we give up).
     * Costs nothing where the window manager already focused us. */
    for (i = 0; i < 10; i++) {
	SDL_PumpEvents ();
	SDL_RaiseWindow (window);
	if (SDL_GetWindowFlags (window) & SDL_WINDOW_INPUT_FOCUS)
	    break;
	SDL_Delay (30);
    }

    /* Linear filtering so downscaling oversized images to fit the window
     * looks smooth rather than aliased. Must be set before any texture is
     * created. */
    SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    /* Create renderer (accelerated if available, else software) */
    renderer = create_renderer (window);
    if (renderer == NULL) {
	SDL_DestroyWindow (window);
	fprintf (stderr, "vp: Unable to create renderer: %s\n", SDL_GetError ());
	free_image_table ();
	return EXIT_FAILURE;
    }
    if (get_state_int (LOUD)) {
	SDL_RendererInfo ri;
	if (SDL_GetRendererInfo (renderer, &ri) == 0)
	    printf ("renderer: %s (%s)\n", ri.name,
		(ri.flags & SDL_RENDERER_ACCELERATED) ? "accelerated" : "software");
    }

    image_freshen ();

    /* -s must stick even when the show doesn't auto-start (single image),
     * so a later space uses it instead of the default. */
    timer_set_interval (wait);
    if (image_table.count > 1)
	timer_start (wait);

    while (!shutdown_requested && handle_input ());

    free_image_table ();
    image_cleanup ();
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
