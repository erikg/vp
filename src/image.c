
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)  *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_image.h>

#include "input.h"
#include "vp.h"
#include "net.h"

extern SDL_Surface *screen;

	/*
	 * dangerous floating point comparison. 
	 */
static double
getscale (double sw, double sh, double iw, double ih)
{
    return (sh * iw < ih * sw) ? sh / ih : sw / iw;
}

	/*
	 * hideous. This should be made more readable, and probably faster.
	 * be nice if it did multi-sampling to get cleaner zooming?
	 */
SDL_Surface *
zoom_blit (SDL_Surface * d, SDL_Surface * s, float scale)
{
    static int x, y, bpp, doff, soff;

    bpp = s->format->BytesPerPixel;

    for (y = 0; y < d->h; y++)
	for (x = 0; x < (d->pitch / bpp); x++)
	{
	    doff = d->pitch * y + x * bpp;
	    soff =
		(int)((int)(s->pitch) * (int)(y / scale)) +
		(bpp * (int)((x) / scale));
	    memcpy ((void *)((int)d->pixels + doff),
		(void *)((int)s->pixels + soff), bpp);
	}
    return d;
}

	/*
	 * ripped from the libsdl faq, 'gtv' code 
	 */
static void
center_window ()
{
    SDL_SysWMinfo info;

    SDL_VERSION (&info.version);

    if (SDL_GetWMInfo (&info) > 0)
    {
	int x, y;
	int w, h;

	if (info.subsystem == SDL_SYSWM_X11)
	{
	    info.info.x11.lock_func ();
	    w = DisplayWidth (info.info.x11.display,
		DefaultScreen (info.info.x11.display));
	    h = DisplayHeight (info.info.x11.display,
		DefaultScreen (info.info.x11.display));
	    x = (w - screen->w) >> 1;
	    y = (h - screen->h) >> 1;
	    XMoveWindow (info.info.x11.display, info.info.x11.wmwindow, x, y);

/*
	    if (get_state_int (GRAB_FOCUS))
		XSetInputFocus (info.info.x11.display, info.info.x11.wmwindow,
		    RevertToNone, CurrentTime);
*/
	    info.info.x11.unlock_func ();
	}
    }
    return;
}

void
image_freshen_sub (struct image_s *i)
{
    if (i->surface == NULL)
    {
	i->surface = IMG_Load (i->resource);
    }
    if (get_state_int (ZOOM))
    {
	double scale =
	    getscale (screen->w, screen->h, i->surface->w, i->surface->h);

	i->scaled = SDL_CreateRGBSurface (SDL_SWSURFACE,
	    (int)ceil ((double)i->surface->w * (double)scale) + 1,
	    (int)ceil ((double)i->surface->h * (double)scale) + 1,
	    i->surface->format->BytesPerPixel * 8,
	    i->surface->format->Rmask, i->surface->format->Gmask,
	    i->surface->format->Bmask, i->surface->format->Amask);
	if (i->scaled->format->BytesPerPixel == 1)
	    memcpy (i->scaled->format->palette, i->surface->format->palette,
		sizeof (SDL_Palette));
	zoom_blit (i->scaled, i->surface, scale);
    }
    return;
}

int
image_freshen ()
{
    struct image_table_s *it = get_image_table ();
    int c = it->current;

    if (c > 1 && it->image[c - 2].surface != NULL)
    {
	SDL_FreeSurface (it->image[c - 2].surface);
	if (it->image[c - 2].scaled)
	    SDL_FreeSurface (it->image[c - 2].scaled);
	it->image[c - 2].surface = NULL;
	it->image[c - 2].scaled = NULL;
    }

    if (c < (it->count - 2) && it->image[c + 2].surface != NULL)
    {
	SDL_FreeSurface (it->image[c + 2].surface);
	if (it->image[c + 2].scaled)
	    SDL_FreeSurface (it->image[c + 2].scaled);
	it->image[c + 2].surface = NULL;
	it->image[c + 2].scaled = NULL;
    }
    image_freshen_sub (&it->image[c]);
    if (c > 0)
	image_freshen_sub (&it->image[c - 1]);
    if (c < (it->count - 1))
	image_freshen_sub (&it->image[c + 1]);
    return 1;
}

int
image_next ()
{
    struct image_table_s *it = get_image_table ();

    if (it->current < (it->count - 1))
	it->count++;
    else
	return 0;
    image_freshen ();
    return 1;
}

int
image_prev ()
{
    struct image_table_s *it = get_image_table ();

    if (it->current > 0)
	it->count--;
    else
	return 0;
    image_freshen ();
    return 1;
}

void
show_image ()
{
    struct image_table_s *it = get_image_table ();
    SDL_Rect r;

    if (get_state_int (LOUD))
	fprintf (stdout, "%s\n", it->image[it->current].resource),
	    fflush (stdout);

    if (get_state_int (FULLSCREEN))
    {
	SDL_FillRect (screen, NULL, 0);
    } else
    {
	static char buffer[1024];

	screen =
	    SDL_SetVideoMode (it->image[it->current].surface->w,
	    it->image[it->current].surface->h, vid_depth (), SDL_DOUBLEBUF);
	sprintf (buffer, "vp - %s", it->image[it->current].resource);
	SDL_WM_SetCaption (buffer, "vp");
	center_window ();
	if (it->image[it->current].surface
	    && it->image[it->current].surface->format)
	{
	    r.x = (Sint16) (screen->w - it->image[it->current].scaled->w) / 2;
	    r.y = (Sint16) (screen->h - it->image[it->current].scaled->h) / 2;
	    r.w = (Uint16) it->image[it->current].scaled->w;
	    r.h = (Uint16) it->image[it->current].scaled->h;
	} else
	    printf ("Image \"%s\" failed\n", it->image[it->current].resource);
    }
    SDL_BlitSurface (it->image[it->current].scaled ? it->image[it->current].
	scaled : it->image[it->current].surface, NULL, screen, &r);
    SDL_Flip (screen);

    return;
}
