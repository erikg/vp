
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_image.h>

#include "input.h"
#include "iview.h"
#include "ll.h"
#include "net.h"

extern SDL_Surface *screen;
SDL_Surface *img = NULL;
static char *imgname = NULL;
char *newname = NULL;
float scale=2;

	/*
	 * dangerous floating point comparison. 
	 */
static double
getscale (double sw, double sh, double iw, double ih)
{
    if ((sw / sh) > (iw / ih))
	return sh / ih;
    return sw / iw;
}

	/*
	 * hideous. This should be made more readable, and probably faster.
	 * be nice if it did multi-sampling to get cleaner zooming?
	 */
void
zoom_blit (SDL_Surface * d, SDL_Surface * s, float scale)
{
    int x, y = 0;

    for (y = 0; y < d->h; y++)
	for (x = 0; x < (d->pitch / 3); x++)
	    memcpy ((void *)((int)(d->pixels) + ((int)(d->pitch) * y) +
		    x * 3),
		(void *)((int)(s->pixels) +
		    (int)((int)(s->pitch) * (int)(y / scale)) +
		    (3 * (int)((x) / scale))), 3);
    return;
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

SDL_Surface *
image_load (char *name)
{
    SDL_Surface *s;

    if (newname == NULL)
    {
	if (net_is_url (name))
	    newname = net_download (name);
	else
	    newname = (char *)strdup (name);
    }
    s = IMG_Load (newname);

/*
    if (strcmp (newname, name))
	net_purge (newname);
*/
    return s;
}

int
image_init (int terminate)
{
    void *imglist = get_imglist ();

    ll_rewind (imglist);
    while ((img = image_load (ll_showline (imglist))) == NULL)
	if (ll_next (imglist) == 0)
	    return -1;
    imgname = ll_showline (imglist);
    return 0;
}

int
img_freshen ()
{
    void *imglist = get_imglist ();

    imgname = ll_showline (imglist);
    if (newname == NULL)
	if ((img = image_load (imgname)) == NULL)
	    return 0;
    return 1;
}

int
image_next (int terminate)
{
    void *imglist = get_imglist ();

    free (newname);
    newname = NULL;
    SDL_FreeSurface (img);
    if (ll_next (imglist) == 0 && terminate == 1)
	return (int)(img = NULL);
    while (!img_freshen ())
	if (ll_next (imglist) == 0 && terminate == 1)
	    return (int)(img = NULL);
    return 1;
}

int
image_prev (int nothing)
{
    void *imglist = get_imglist ();

    free (newname);
    newname = NULL;
    SDL_FreeSurface (img);
    ll_prev (imglist);
    while (!img_freshen ())
	if (ll_prev (imglist) == 0)
	    return 1;
    return 1;
}

void
show_image ()
{
    SDL_Surface *buf = NULL;
    float scale;
    SDL_Rect r;

    if (img == NULL)
    {
	throw_exit ();
	return;
    }
    /*
     * maybe this should be elsewhere? 
     */
    if (get_state_int (LOUD))
	fprintf (stdout, "%s\n", imgname), fflush (stdout);

    if (!get_state_int (SDL_FULLSCREEN))
    {
	if (img)
	{
	    char buffer[1024];

	    screen = SDL_SetVideoMode (img->w, img->h, 32, SDL_DOUBLEBUF);
	    sprintf (buffer, "iview - %s", imgname);
	    SDL_WM_SetCaption (buffer, "iview");
	}
	buf = img;
	center_window ();
    }
    if (get_state_int (ZOOM))
    {
	scale = getscale (screen->w, screen->h, img->w, img->h);
}
	if (img && img->format)
	    buf = SDL_CreateRGBSurface (SDL_SWSURFACE,
		img->w * scale,
		img->h * scale,
		img->format->BytesPerPixel * 8,
		img->format->Rmask,
		img->format->Gmask, img->format->Bmask, img->format->Amask);
	zoom_blit (buf, img, scale);
    SDL_FillRect (screen, NULL, 0);
    r.x = (Sint16) (screen->w - buf->w) / 2;
    r.y = (Sint16) (screen->h - buf->h) / 2;
    r.w = (Uint16) buf->w;
    r.h = (Uint16) buf->h;
    SDL_BlitSurface (buf, NULL, screen, &r);
    SDL_Flip (screen);
    if (buf != img)
	SDL_FreeSurface (buf);
    buf = NULL;
    return;
}
