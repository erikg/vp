
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

#include <SDL.h>
#include "image.h"
#include "input.h"
#include "iview.h"
#include "timer.h"

	/*
	 * these are ugly and shouldn't be here. Must redesign. 
	 */
extern SDL_Surface *screen, *img;

	/*
	 * instead of actually exiting, we just fake the escape key 
	 */
void
throw_exit ()
{
    SDL_Event thrower;

    thrower.type = SDL_KEYDOWN;
    thrower.key.keysym.sym = 27;
    timer_stop ();
    SDL_PushEvent (&thrower);
}

int
handle_input ()
{
    SDL_Event e;

    SDL_WaitEvent (&e);
    switch (e.type)
    {
    case SDL_KEYDOWN:
	switch (e.key.keysym.sym)
	{
	case 'Q':
	case 'q':
	case SDLK_ESCAPE:
	    return 0;
	    break;
	case SDLK_SPACE:
	    timer_toggle ();
	    break;
	case SDLK_RETURN:
	    show_image ();
	    timer_stop ();
	    break;
	case SDLK_RIGHT:
	    timer_stop ();
	    image_next (0);
	    show_image ();
	    break;
	case SDLK_LEFT:
	    timer_stop ();
	    image_prev (0);
	    show_image ();
	    break;
	case 'z':
	case 'Z':
	    timer_stop ();
	    toggle_state (ZOOM);
img_freshen();
	    show_image ();
	    break;
	case 'f':
	case 'F':
	    timer_stop ();
	    toggle_state (SDL_FULLSCREEN);
	    if (!get_state_int (SDL_FULLSCREEN))
		screen = SDL_SetVideoMode (img->w, img->h, 32, 0);
	    else
		screen = SDL_SetVideoMode (1024, 768, 32, SDL_FULLSCREEN);
	    show_image ();
	    break;
	default:
	    printf ("Unknown keysym: %d '%c'\n", e.key.keysym.sym,
		    e.key.keysym.sym);
	    break;
	}
	break;
    case SDL_QUIT:
	return 0;;
	break;
    }
    return 1;
}
