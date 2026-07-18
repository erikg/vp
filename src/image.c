
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)     *
 * Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>                   *
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
#include <string.h>
#include <limits.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_image.h>

#include "vp.h"
#include "image.h"
#include "font8x8.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_mutex *mutex;

/* On-screen-display anchor. Cycled by the shift-N key (osd_cycle_position). */
enum {
    OSD_TL, OSD_TC, OSD_TR,
    OSD_BL, OSD_BC, OSD_BR,
    OSD_CC,
    OSD_POS_COUNT
};
static int osd_pos = OSD_BL;

void
osd_cycle_position (void)
{
    osd_pos = (osd_pos + 1) % OSD_POS_COUNT;
    return;
}

/*
 * Draw a NUL-terminated string with the embedded 8x8 font at (x,y), each font
 * pixel drawn as a scale x scale filled rect in the current draw color.
 */
static void
draw_string (int x, int y, int scale, const char *text)
{
    const unsigned char *p;
    SDL_Rect px;

    px.w = px.h = scale;
    for (p = (const unsigned char *) text; *p; p++)
    {
	const unsigned char *glyph = font8x8_basic[*p & 0x7f];
	int row, col;

	for (row = 0; row < 8; row++)
	{
	    unsigned char bits = glyph[row];

	    for (col = 0; col < 8; col++)
		if (bits & (1 << col))
		{
		    px.x = x + col * scale;
		    px.y = y + row * scale;
		    SDL_RenderFillRect (renderer, &px);
		}
	}
	x += 8 * scale;
    }
    return;
}

/*
 * Render the OSD filename overlay: a translucent backing bar for legibility
 * over busy images, then the text, anchored per osd_pos. Called from
 * show_image() after the image is drawn and before RenderPresent.
 */
static void
draw_osd (const char *text)
{
    int win_w, win_h, scale, tw, th, pad, x, y;
    SDL_Rect bar;

    if (text == NULL || *text == '\0')
	return;

    SDL_GetWindowSize (window, &win_w, &win_h);

    /* Auto-size the glyphs from window height; readable but unobtrusive. */
    scale = win_h / 360;
    if (scale < 1)
	scale = 1;

    tw = (int) strlen (text) * 8 * scale;
    th = 8 * scale;
    pad = 4 * scale;

    switch (osd_pos)
    {
    case OSD_TL:	x = pad;			y = pad;		break;
    case OSD_TC:	x = (win_w - tw) / 2;		y = pad;		break;
    case OSD_TR:	x = win_w - tw - pad;		y = pad;		break;
    case OSD_BL:	x = pad;			y = win_h - th - pad;	break;
    case OSD_BC:	x = (win_w - tw) / 2;		y = win_h - th - pad;	break;
    case OSD_BR:	x = win_w - tw - pad;		y = win_h - th - pad;	break;
    case OSD_CC:
    default:		x = (win_w - tw) / 2;		y = (win_h - th) / 2;	break;
    }

    /* A name too wide for the window would push centered/right anchors off
     * the left edge; pin it so at least the start of the name is readable. */
    if (x < pad)
	x = pad;
    if (y < pad)
	y = pad;

    /* Translucent black backing bar. */
    bar.x = x - pad;
    bar.y = y - pad;
    bar.w = tw + 2 * pad;
    bar.h = th + 2 * pad;
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 160);
    SDL_RenderFillRect (renderer, &bar);

    /* Foreground text. */
    SDL_SetRenderDrawColor (renderer, 255, 255, 255, 255);
    draw_string (x, y, scale, text);
    return;
}

/* Flush the X connection. Was called sync() for 20+ years, which collided
 * with (and linker-shadowed) POSIX sync(2); nothing here wants that. */
static void
x_sync (void)
{
#ifdef SDL_SYSWM_X11
    SDL_SysWMinfo info;

    SDL_VERSION (&info.version);
    if (SDL_GetWindowWMInfo (window, &info) > 0)
    {
	if (info.subsystem == SDL_SYSWM_X11)
	    XSync (info.info.x11.display, False);
    }
#endif
    return;
}

static double
getscale (double sw, double sh, double iw, double ih)
{
    /* Prevent division by zero */
    if (ih <= 0.0 || iw <= 0.0) {
	return 1.0;
    }
    return (sh * iw < ih * sw) ? sh / ih : sw / iw;
}

void show_image (void);

/*
 * View transform: the displayed image is the current surface scaled by
 * view_scale (1.0 = one image pixel per screen pixel) with its top-left
 * corner at (view_x, view_y) in window coordinates. view_manual is set once
 * the user pans or zooms; until then show_image() recomputes the automatic
 * layout (fit or 1:1) every draw. The scaling itself happens in the
 * RenderCopy dst rect, so the GPU (or SDL's software renderer) does the
 * work with the linear filter hint set at startup.
 */
#define VIEW_SCALE_MIN	0.03125
#define VIEW_SCALE_MAX	32.0

static double view_scale = 1.0;
static int view_x = 0, view_y = 0;
static int view_manual = 0;

/* Which image the window was last laid out (sized/titled/centered) for;
 * -1 forces a relayout and an automatic-view reset on the next draw. */
static int last_layout = -1;

/* The current image's texture, uploaded once per image instead of on every
 * pan/zoom redraw. */
static SDL_Texture *cur_tex = NULL;
static SDL_Surface *cur_tex_src = NULL;
static int cur_tex_idx = -1;

/*
 * Keep the view on the image: an axis whose scaled size fits in the window
 * is centered; one that overflows may pan, but never far enough to open a
 * gap between the image edge and the window edge.
 */
static void
view_clamp (void)
{
    struct image_table_s *it = get_image_table ();
    SDL_Surface *s;
    int win_w, win_h, sw, sh;

    if (it->current < 0 || it->current >= it->count)
	return;
    s = it->image[it->current].surface;
    if (s == NULL)
	return;

    SDL_GetWindowSize (window, &win_w, &win_h);
    sw = (int) (s->w * view_scale);
    sh = (int) (s->h * view_scale);

    if (sw <= win_w)
	view_x = (win_w - sw) / 2;
    else if (view_x > 0)
	view_x = 0;
    else if (view_x < win_w - sw)
	view_x = win_w - sw;

    if (sh <= win_h)
	view_y = (win_h - sh) / 2;
    else if (view_y > 0)
	view_y = 0;
    else if (view_y < win_h - sh)
	view_y = win_h - sh;
    return;
}

void
view_pan (int dx, int dy)
{
    SDL_LockMutex (mutex);
    view_manual = 1;
    view_x += dx;
    view_y += dy;
    view_clamp ();
    show_image ();
    SDL_UnlockMutex (mutex);
    return;
}

void
view_zoom (double factor, int ax, int ay)
{
    double ns;

    SDL_LockMutex (mutex);
    ns = view_scale * factor;
    if (ns < VIEW_SCALE_MIN)
	ns = VIEW_SCALE_MIN;
    if (ns > VIEW_SCALE_MAX)
	ns = VIEW_SCALE_MAX;

    /* Keep the image point under the anchor (ax,ay) fixed on screen. */
    view_x = ax - (int) ((ax - view_x) * (ns / view_scale));
    view_y = ay - (int) ((ay - view_y) * (ns / view_scale));
    view_scale = ns;
    view_manual = 1;
    view_clamp ();
    show_image ();
    SDL_UnlockMutex (mutex);
    return;
}

void
view_actual_size (void)
{
    int win_w, win_h;

    SDL_GetWindowSize (window, &win_w, &win_h);
    view_zoom (1.0 / view_scale, win_w / 2, win_h / 2);
    return;
}

void
view_reset (void)
{
    view_manual = 0;
    last_layout = -1;
    return;
}

void
image_cleanup (void)
{
    if (cur_tex)
	SDL_DestroyTexture (cur_tex);
    cur_tex = NULL;
    cur_tex_src = NULL;
    cur_tex_idx = -1;
    return;
}

/*
 * Largest window content size that fits on the current display without the
 * window running off-screen: the usable desktop area (which already excludes
 * the dock / taskbar / panels) minus the window-manager decorations (title bar
 * and borders). Border reporting is best-effort - it returns zeros on WMs or
 * backends (e.g. Wayland) that don't expose it, and before the window is first
 * mapped - so we just under-subtract in those cases, never overrun.
 */
static void
window_max_content (int *max_w, int *max_h)
{
    SDL_Rect usable;
    int disp, top = 0, left = 0, bottom = 0, right = 0;

    disp = SDL_GetWindowDisplayIndex (window);
    if (disp < 0)
	disp = 0;

    if (SDL_GetDisplayUsableBounds (disp, &usable) != 0 &&
	SDL_GetDisplayBounds (disp, &usable) != 0)
    {
	/* No display geometry available; don't clamp. */
	*max_w = *max_h = INT_MAX;
	return;
    }

    SDL_GetWindowBordersSize (window, &top, &left, &bottom, &right);

    *max_w = usable.w - left - right;
    *max_h = usable.h - top - bottom;
    if (*max_w < 1)
	*max_w = 1;
    if (*max_h < 1)
	*max_h = 1;
    return;
}

	/*
	 * ripped from the libsdl faq, 'gtv' code
	 */
static void
center_window (void)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return;
}

void
show_image (void)
{
    struct image_table_s *it = get_image_table ();
    SDL_Rect r;
    SDL_Surface *s;
    int window_w, window_h;

    /* Check bounds before accessing array */
    if (it->current < 0 || it->current >= it->count) {
	return;
    }

    if (get_state_int (LOUD))
    {
	fprintf (stdout, "%s\n", it->image[it->current].resource);
	fflush (stdout);
    }

    s = it->image[it->current].surface;
    if (s == NULL || s->format == NULL)
    {
	printf ("Image \"%s\" failed\n", it->image[it->current].resource);
	return;
    }

    /* New image (or forced relayout): drop any manual pan/zoom and, in
     * windowed mode, size the window to the image - but never larger than
     * the display can show (usable desktop minus decorations), so oversized
     * images don't spawn a window that runs off-screen. Pan/zoom redraws of
     * the same image skip all of this. */
    if (it->current != last_layout)
    {
	view_manual = 0;
	if (!get_state_int (FULLSCREEN))
	{
	    char buffer[BUFSIZ];  /* Local buffer to prevent race conditions */
	    int max_w, max_h, win_w, win_h;

	    window_max_content (&max_w, &max_h);
	    win_w = s->w < max_w ? s->w : max_w;
	    win_h = s->h < max_h ? s->h : max_h;

	    SDL_SetWindowSize(window, win_w, win_h);
	    snprintf (buffer, BUFSIZ, "vp - %s", it->image[it->current].resource);
	    SDL_SetWindowTitle(window, buffer);
	    center_window ();
	}
	last_layout = it->current;
    }

    SDL_GetWindowSize(window, &window_w, &window_h);

    /* Automatic layout until the user pans or zooms: fit when the zoom
     * toggle is on, else 1:1 in fullscreen (a too-big image is cropped) or
     * shrink-to-fit in windowed mode (never enlarge), centered either way. */
    if (!view_manual)
    {
	double fit = getscale (window_w, window_h, s->w, s->h);

	if (get_state_int (ZOOM))
	    view_scale = fit;
	else if (get_state_int (FULLSCREEN))
	    view_scale = 1.0;
	else
	    view_scale = fit < 1.0 ? fit : 1.0;
	view_x = (window_w - (int) (s->w * view_scale)) / 2;
	view_y = (window_h - (int) (s->h * view_scale)) / 2;
    }

    /* Clear to black explicitly: draw_osd() leaves the draw color white, and
     * RenderClear uses the draw color, so pin it here for the letterbox. */
    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Upload the texture once per image, not on every pan/zoom redraw. */
    if (cur_tex == NULL || cur_tex_idx != it->current || cur_tex_src != s)
    {
	if (cur_tex)
	    SDL_DestroyTexture (cur_tex);
	cur_tex = SDL_CreateTextureFromSurface (renderer, s);
	cur_tex_src = s;
	cur_tex_idx = it->current;
    }

    r.x = view_x;
    r.y = view_y;
    r.w = (int) (s->w * view_scale);
    r.h = (int) (s->h * view_scale);
    if (r.w < 1)
	r.w = 1;
    if (r.h < 1)
	r.h = 1;

    if (cur_tex)
	SDL_RenderCopy (renderer, cur_tex, NULL, &r);

    if (get_state_int (OSD))
	draw_osd (it->image[it->current].resource);

    SDL_RenderPresent(renderer);
    return;
}

/*
 * Ensure a table entry has its decoded surface resident. All scaling is
 * done at draw time by the renderer, so this is the whole load step.
 */
static void
image_load_surface (struct image_s *i)
{
    if (i->surface == NULL)
	i->surface = IMG_Load (i->file);
    return;
}

int
image_freshen (void)
{
    struct image_table_s *it = get_image_table ();
    int c, lo, hi, idx;

    SDL_LockMutex (mutex);

    x_sync ();
    c = it->current;

    /* Check bounds before accessing array */
    if (c < 0 || c >= it->count) {
	SDL_UnlockMutex (mutex);
	return 0;
    }

    /* Sliding-window cache: keep the current image plus up to 2 before and
     * 2 after it decoded in memory (clamped to the ends of the list), so
     * paging back and forth across the same handful of images doesn't
     * re-decode from disk every time. Anything outside that window gets its
     * surfaces freed. */
    lo = c - 2;
    if (lo < 0)
	lo = 0;
    hi = c + 2;
    if (hi > it->count - 1)
	hi = it->count - 1;

    for (idx = 0; idx < it->count; idx++)
    {
	struct image_s *i = &it->image[idx];

	if (idx < lo || idx > hi)
	{
	    if (i->surface)
		SDL_FreeSurface (i->surface);
	    i->surface = NULL;
	} else
	    image_load_surface (i);
    }

    show_image ();
    x_sync ();
    SDL_UnlockMutex (mutex);
    return 1;
}

int
image_next (void)
{
    struct image_table_s *it = get_image_table ();

    SDL_LockMutex (mutex);

    if (it->current < (it->count - 1))
	it->current++;
    else {
	SDL_UnlockMutex (mutex);
	return 0;
    }
    SDL_UnlockMutex (mutex);
    image_freshen ();
    return 1;
}

int
image_prev (void)
{
    struct image_table_s *it = get_image_table ();

    SDL_LockMutex (mutex);

    if (it->current > 0)
	it->current--;
    else {
	SDL_UnlockMutex (mutex);
	return 0;
    }
    SDL_UnlockMutex (mutex);
    image_freshen ();
    return 1;
}
