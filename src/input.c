
/*****************************************************************************
 * vp    -    SDL based image viewer for linux and fbsd. (X and console)     *
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
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 ****************************************************************************/

#include <math.h>
#include <SDL.h>
#include "image.h"
#include "input.h"
#include "vp.h"
#include "timer.h"

	/*
	 * these are ugly and shouldn't be here. Must redesign.
	 */
extern SDL_Window *window;
extern SDL_Renderer *renderer;

	/*
	 * mouse state for pan/zoom dragging. The chord check treats
	 * left+right held together as a middle button, for two-button mice.
	 */
static Uint32 buttons = 0;
static int zoom_anchor_x = 0, zoom_anchor_y = 0;

#define CHORD_MIDDLE(b) (((b) & SDL_BUTTON_MMASK) || \
	(((b) & SDL_BUTTON_LMASK) && ((b) & SDL_BUTTON_RMASK)))

	/*
	 * keyboard zoom, anchored at the window center
	 */
static void
key_zoom (double factor)
{
    int w, h;

    SDL_GetWindowSize (window, &w, &h);
    timer_stop ();
    view_zoom (factor, w / 2, h / 2);
    return;
}

	/*
	 * keyboard pan step: an eighth of the window dimension, at least 32px
	 */
static int
pan_step (int dim)
{
    int s = dim / 8;

    return s < 32 ? 32 : s;
}

	/*
	 * instead of actually exiting, we just fake the escape key
	 */
void
throw_exit (void)
{
    SDL_Event thrower;

    printf ("Throwing ext\n");
    SDL_zero (thrower);		/* no garbage in the fields we don't set */
    thrower.type = SDL_KEYDOWN;
    thrower.key.keysym.sym = SDLK_ESCAPE;
    timer_stop ();
    SDL_PushEvent (&thrower);
}

int
handle_input (void)
{
    SDL_Event e;

    /* Timed wait (not SDL_WaitEvent) so the caller's loop periodically
     * re-checks shutdown_requested set by the signal handler. A timeout
     * (return 0) just means "no event this tick" - keep going. */
    if (!SDL_WaitEventTimeout (&e, 200))
	return 1;
    switch (e.type)
    {
    case SDL_QUIT:
	/* Window close button or signal handler */
	return 0;
	/*
	 * thanks to Ted Mielczarek <tam4@lehigh.edu> for this, fixes the X
	 * Async request errors
	 */
    case SDL_USEREVENT:
	if (e.user.code == SHOW_IMAGE)
	    image_freshen ();
	else if (e.user.code == NEXT_IMAGE) {
	    /* A queued tick can outlive timer_stop() - by queue ordering, or
	     * because SDL_RemoveTimer doesn't wait out a callback already in
	     * flight. Drop it, so pausing on the last image pauses instead of
	     * falling through to exit. */
	    if (timer_running () && image_next () == 0)
		throw_exit ();
	}
	break;
    case SDL_MOUSEBUTTONDOWN:
	timer_stop ();
	buttons |= SDL_BUTTON (e.button.button);
	zoom_anchor_x = e.button.x;
	zoom_anchor_y = e.button.y;
	break;
    case SDL_MOUSEBUTTONUP:
	buttons &= ~SDL_BUTTON (e.button.button);
	break;
    case SDL_MOUSEMOTION:
	/* middle-drag (or the left+right chord) zooms about the point where
	 * the drag started, ~1% per pixel of vertical motion, up to zoom in;
	 * a plain left-drag pans. */
	if (CHORD_MIDDLE (buttons))
	{
	    if (e.motion.yrel)
		view_zoom (pow (1.01, (double) -e.motion.yrel),
		    zoom_anchor_x, zoom_anchor_y);
	} else if (buttons & SDL_BUTTON_LMASK)
	    view_pan (e.motion.xrel, e.motion.yrel);
	break;
    case SDL_MOUSEWHEEL:
	{
	    int mx, my;

	    timer_stop ();
	    SDL_GetMouseState (&mx, &my);
	    if (e.wheel.y > 0)
		view_zoom (1.2, mx, my);
	    else if (e.wheel.y < 0)
		view_zoom (1.0 / 1.2, mx, my);
	}
	break;
    case SDL_KEYDOWN:
    {
	/* SDL2 keycodes for letters are already lowercase; fold A-Z anyway
	 * for safety. (tolower() on non-char keycodes like SDLK_RIGHT is
	 * formally UB - glibc/FreeBSD range-guard it, but don't rely on
	 * that.) */
	SDL_Keycode sym = e.key.keysym.sym;

	if (sym >= 'A' && sym <= 'Z')
	    sym += 'a' - 'A';
	switch (sym)
	{
	case 'x':
	case 'q':
	case SDLK_ESCAPE:
	    return 0;
	    break;
	case SDLK_SPACE:
	    timer_toggle ();
	    break;
	case SDLK_RETURN:
	    image_freshen ();
	    timer_stop ();
	    break;
	case SDLK_RIGHT:
	    /* plain arrows change image; shifted arrows pan the view */
	    timer_stop ();
	    if (e.key.keysym.mod & KMOD_SHIFT)
	    {
		int w;

		SDL_GetWindowSize (window, &w, NULL);
		view_pan (-pan_step (w), 0);
	    } else
		image_next ();
	    break;
	case SDLK_LEFT:
	    timer_stop ();
	    if (e.key.keysym.mod & KMOD_SHIFT)
	    {
		int w;

		SDL_GetWindowSize (window, &w, NULL);
		view_pan (pan_step (w), 0);
	    } else
		image_prev ();
	    break;
	case SDLK_UP:
	    if (e.key.keysym.mod & KMOD_SHIFT)
	    {
		int h;

		SDL_GetWindowSize (window, NULL, &h);
		timer_stop ();
		view_pan (0, pan_step (h));
	    }
	    break;
	case SDLK_DOWN:
	    if (e.key.keysym.mod & KMOD_SHIFT)
	    {
		int h;

		SDL_GetWindowSize (window, NULL, &h);
		timer_stop ();
		view_pan (0, -pan_step (h));
	    }
	    break;
	case '=':
	    /* = resets to 1:1; shift-= is + on most layouts, so zoom in */
	    if (e.key.keysym.mod & KMOD_SHIFT)
		key_zoom (1.1);
	    else
	    {
		timer_stop ();
		view_actual_size ();
	    }
	    break;
	case '+':
	case SDLK_KP_PLUS:
	    key_zoom (1.1);
	    break;
	case '-':
	case SDLK_KP_MINUS:
	    key_zoom (1.0 / 1.1);
	    break;
	case SDLK_PAGEUP:
	    key_zoom (2.0);
	    break;
	case SDLK_PAGEDOWN:
	    key_zoom (0.5);
	    break;
	case 'z':
	    timer_stop ();
	    toggle_state (ZOOM);
	    view_reset ();
	    image_freshen ();
	    break;
	case 'f':
	    timer_stop ();
	    toggle_state (FULLSCREEN);
	    if (get_state_int (FULLSCREEN)) {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_ShowCursor (0);
	    } else {
		SDL_SetWindowFullscreen(window, 0);
		SDL_ShowCursor (1);
	    }
	    view_reset ();
	    image_freshen ();
	    break;
	case 'n':
	    /* n toggles the filename OSD; shift-N cycles its position. */
	    if (e.key.keysym.mod & KMOD_SHIFT)
		osd_cycle_position ();
	    else
		toggle_state (OSD);
	    image_freshen ();
	    break;
	default:
	    /*
	     * do nothing
	     */
	    break;
	}
	break;
    }
    }
    return 1;
}
