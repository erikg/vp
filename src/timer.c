
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
#include "ll.h"
#include "timer.h"

#define MILLIS 2500

SDL_TimerID timer_id;

extern void *imglist;

int
timer_stub ()
{
    if (image_next (1) == 0)
	throw_exit ();
    show_image ();
    return MILLIS;
}

void
timer_toggle ()
{
    if (timer_id == 0)
	timer_start ();
    else
	timer_stop ();
    return;
}

void
timer_stop ()
{
    if (timer_id != 0)
	if (SDL_RemoveTimer (timer_id) == SDL_FALSE)
	    oops ("SDL_RemoveTimer() failed\n");

    timer_id = 0;
    return;
}

void
timer_start ()
{
    if (timer_id == 0)
	timer_id =
	    SDL_AddTimer (MILLIS, (SDL_NewTimerCallback) timer_stub, NULL);
//    show_image ();
    return;
}
