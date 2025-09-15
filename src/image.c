
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_image.h>

#include "vp.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_mutex *mutex;

void
sync ()
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

static double
getscale_fill (double sw, double sh, double iw, double ih)
{
    /* Prevent division by zero */
    if (ih <= 0.0 || iw <= 0.0) {
	return 1.0;
    }
    /* Choose the larger scale factor to fill the screen while preserving aspect ratio */
    double scale_w = sw / iw;
    double scale_h = sh / ih;
    return (scale_w > scale_h) ? scale_w : scale_h;
}

	/*
	 * hideous. This should be made more readable, and probably faster.
	 * be nice if it did multi-sampling to get cleaner zooming?
	 */
SDL_Surface *
zoom_blit (SDL_Surface * d, SDL_Surface * s, float scale)
{
    size_t x, y, bpp, doff, soff, width;

    /* Validate parameters */
    if (d == NULL || s == NULL || s->format == NULL) {
	return d;
    }

    /* Prevent division by zero */
    if (scale <= 0.0f) {
	return d;
    }

    bpp = s->format->BytesPerPixel;
    width = d->w;

    for (y = 0; y < d->h; y++)
	for (x = 0; x < width; x++)
	{
	    size_t src_x = (size_t)(x / scale);
	    size_t src_y = (size_t)(y / scale);

	    /* Check source bounds to prevent buffer overflow */
	    if (src_x >= s->w || src_y >= s->h) {
		continue;
	    }

	    /* Check for integer overflow in offset calculations */
	    if (y > SIZE_MAX / d->pitch || x > (SIZE_MAX - d->pitch * y) / bpp) {
		continue;
	    }
	    if (src_y > SIZE_MAX / s->pitch || src_x > (SIZE_MAX - s->pitch * src_y) / bpp) {
		continue;
	    }

	    doff = d->pitch * y + x * bpp;
	    soff = s->pitch * src_y + src_x * bpp;
/* TODO this pointer casting causes warnings on 64b */
	    memcpy ((void *)((size_t)d->pixels + doff),
		(void *)((size_t)s->pixels + soff), bpp);
	}
    return d;
}

SDL_Surface *
zoom_blit_fill (SDL_Surface * d, SDL_Surface * s, float scale)
{
    size_t x, y, bpp, doff, soff, width, height;
    int scaled_w, scaled_h, offset_x, offset_y;

    /* Validate parameters */
    if (d == NULL || s == NULL || s->format == NULL) {
	return d;
    }

    /* Prevent division by zero */
    if (scale <= 0.0f) {
	return d;
    }

    bpp = s->format->BytesPerPixel;
    width = d->w;
    height = d->h;

    /* Calculate scaled dimensions and centering offset for cropping with overflow protection */
    double scaled_w_f = (double)s->w * (double)scale;
    double scaled_h_f = (double)s->h * (double)scale;

    /* Check for overflow before casting to int */
    if (scaled_w_f > INT_MAX || scaled_h_f > INT_MAX || scaled_w_f < 0 || scaled_h_f < 0) {
	return d;  /* Overflow - return unchanged */
    }

    scaled_w = (int)scaled_w_f;
    scaled_h = (int)scaled_h_f;
    offset_x = (scaled_w - d->w) / 2;  /* Amount to crop from left/right */
    offset_y = (scaled_h - d->h) / 2;  /* Amount to crop from top/bottom */

    /* Fill the entire destination surface */
    for (y = 0; y < height; y++)
	for (x = 0; x < width; x++)
	{
	    /* Calculate source coordinates accounting for cropping offset */
	    int src_x = (int)((x + offset_x) / scale);
	    int src_y = (int)((y + offset_y) / scale);

	    /* Check bounds - only copy if source coordinates are valid */
	    if (src_x >= 0 && src_x < s->w && src_y >= 0 && src_y < s->h)
	    {
		/* Check for integer overflow in offset calculations */
		if (y > SIZE_MAX / d->pitch || x > (SIZE_MAX - d->pitch * y) / bpp) {
		    continue;
		}
		if (src_y > SIZE_MAX / s->pitch || src_x > (SIZE_MAX - s->pitch * src_y) / bpp) {
		    continue;
		}

		doff = d->pitch * y + x * bpp;
		soff = s->pitch * src_y + src_x * bpp;
		memcpy ((void *)((size_t)d->pixels + doff),
			(void *)((size_t)s->pixels + soff), bpp);
	    }
	}
    return d;
}

SDL_Surface *
zoom_blit_centered (SDL_Surface * d, SDL_Surface * s, float scale)
{
    size_t x, y, bpp, doff, soff, width, height;
    int scaled_w, scaled_h, offset_x, offset_y;

    /* Validate parameters */
    if (d == NULL || s == NULL || s->format == NULL) {
	return d;
    }

    /* Prevent division by zero */
    if (scale <= 0.0f) {
	return d;
    }

    bpp = s->format->BytesPerPixel;
    width = d->w;
    height = d->h;

    /* Calculate scaled dimensions and centering offset with overflow protection */
    double scaled_w_f = (double)s->w * (double)scale;
    double scaled_h_f = (double)s->h * (double)scale;

    /* Check for overflow before casting to int */
    if (scaled_w_f > INT_MAX || scaled_h_f > INT_MAX || scaled_w_f < 0 || scaled_h_f < 0) {
	return d;  /* Overflow - return unchanged */
    }

    scaled_w = (int)scaled_w_f;
    scaled_h = (int)scaled_h_f;
    offset_x = (d->w - scaled_w) / 2;
    offset_y = (d->h - scaled_h) / 2;

    /* Clear the destination surface first */
    SDL_FillRect(d, NULL, 0);

    for (y = 0; y < height; y++)
	for (x = 0; x < width; x++)
	{
	    /* Calculate source coordinates accounting for centering */
	    int src_x = (int)((x - offset_x) / scale);
	    int src_y = (int)((y - offset_y) / scale);

	    /* Check bounds - only copy if source coordinates are valid */
	    if (src_x >= 0 && src_x < s->w && src_y >= 0 && src_y < s->h &&
		x >= offset_x && x < (offset_x + scaled_w) &&
		y >= offset_y && y < (offset_y + scaled_h))
	    {
		/* Check for integer overflow in offset calculations */
		if (y > SIZE_MAX / d->pitch || x > (SIZE_MAX - d->pitch * y) / bpp) {
		    continue;
		}
		if (src_y > SIZE_MAX / s->pitch || src_x > (SIZE_MAX - s->pitch * src_y) / bpp) {
		    continue;
		}

		doff = d->pitch * y + x * bpp;
		soff = s->pitch * src_y + src_x * bpp;
		memcpy ((void *)((size_t)d->pixels + doff),
			(void *)((size_t)s->pixels + soff), bpp);
	    }
	}
    return d;
}

	/*
	 * ripped from the libsdl faq, 'gtv' code
	 */
static void
center_window ()
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return;
}

void
show_image ()
{
    struct image_table_s *it = get_image_table ();
    SDL_Rect r;
    SDL_Surface *s;
    SDL_Texture *texture;
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
    if (s == NULL)
	return;

    SDL_RenderClear(renderer);

    if (get_state_int (FULLSCREEN))
    {
	if (get_state_int (ZOOM))
	    s = it->image[it->current].scaled;
    } else
    {
	char buffer[BUFSIZ];  /* Local buffer to prevent race conditions */

	SDL_SetWindowSize(window, s->w, s->h);
	snprintf (buffer, BUFSIZ, "vp - %s", it->image[it->current].resource);
	SDL_SetWindowTitle(window, buffer);
	center_window ();
    }

    if (s && s->format)
    {
	SDL_GetWindowSize(window, &window_w, &window_h);

	/* Center the image on screen */
	r.x = (window_w - s->w) / 2;
	r.y = (window_h - s->h) / 2;
	r.w = s->w;
	r.h = s->h;

	texture = SDL_CreateTextureFromSurface(renderer, s);
	if (texture) {
	    SDL_RenderCopy(renderer, texture, NULL, &r);
	    SDL_DestroyTexture(texture);
	}
    } else
	printf ("Image \"%s\" failed\n", it->image[it->current].resource);

    SDL_RenderPresent(renderer);
    return;
}

/* saw a crash in here on g5 -fast 
 * Exception:  EXC_BAD_ACCESS (0x0001)
 * Codes:      KERN_INVALID_ADDRESS (0x0001) at 0x02730003
 *
 * Thread 0 Crashed:
 * 0   <<00000000>>	0xffff8834 __memcpy + 148 (cpu_capabilities.h:189)
 * 1   vp				0x00003154 image_freshen_sub + 836
 * 2   vp				0x00003284 image_freshen + 212
 * 3   vp				0x00003310 image_prev + 80
 */
void
image_freshen_sub (struct image_s *i)
{
    if (i->surface == NULL)
    {
	i->surface = IMG_Load (i->file);
	if (i->surface == NULL) {
	    return;
	}
    }
    if (i->scaled == NULL && get_state_int (ZOOM))
    {
	int window_w, window_h;
	SDL_GetWindowSize(window, &window_w, &window_h);
	double scale;

	/* Fit within window while preserving aspect ratio (letterbox/pillarbox) */
	scale = getscale (window_w, window_h, i->surface->w, i->surface->h);

	/* Check for integer overflow in scaled dimensions */
	double scaled_w = ceil ((double)i->surface->w * (double)scale) + 1;
	double scaled_h = ceil ((double)i->surface->h * (double)scale) + 1;

	if (scaled_w > INT_MAX || scaled_h > INT_MAX || scaled_w <= 0 || scaled_h <= 0) {
	    return;
	}

	i->scaled = SDL_CreateRGBSurface (0,
	    (int)scaled_w, (int)scaled_h,
	    i->surface->format->BytesPerPixel * 8,
	    i->surface->format->Rmask, i->surface->format->Gmask,
	    i->surface->format->Bmask, i->surface->format->Amask);

	if (i->scaled == NULL) {
	    return;
	}

	/* Set palette if needed */
	if (i->scaled->format->BytesPerPixel == 1 && i->surface->format->palette)
	    SDL_SetPaletteColors(i->scaled->format->palette,
				 i->surface->format->palette->colors, 0,
				 i->surface->format->palette->ncolors);

	/* Scale the image using standard zoom_blit */
	zoom_blit (i->scaled, i->surface, scale);
    }
    return;
}

int
image_freshen ()
{
    struct image_table_s *it = get_image_table ();
    int c;

    SDL_LockMutex (mutex);

    sync ();
    c = it->current;

    /* Check bounds before accessing array */
    if (c < 0 || c >= it->count) {
	SDL_UnlockMutex (mutex);
	return 0;
    }

    if (c > 0)
    {
	struct image_s *i = &it->image[c - 1];

	if (i->surface)
	    SDL_FreeSurface (i->surface);
	if (i->scaled)
	    SDL_FreeSurface (i->scaled);
	i->surface = i->scaled = NULL;
    }
    if (c < (it->count - 1))
    {
	struct image_s *i = &it->image[c + 1];

	if (i->surface)
	    SDL_FreeSurface (i->surface);
	if (i->scaled)
	    SDL_FreeSurface (i->scaled);
	i->surface = i->scaled = NULL;
    }

    image_freshen_sub (&it->image[c]);
    show_image ();
    sync ();
    SDL_UnlockMutex (mutex);
    return 1;
}

int
image_next ()
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
image_prev ()
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
