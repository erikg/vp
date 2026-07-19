
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

#include <SDL.h>

#include "image.h"
#include "input.h"
#include "vp.h"
#include "timer.h"

static int wait_time = 2500;

SDL_TimerID timer_id = 0;

static Uint32
timer_stub (Uint32 interval, void *param)
{
    (void)interval;
    (void)param;

    SDL_Event ev;

    /*
     * Instead of calling image_next() directly from timer thread,
     * send an event to the main thread for safe processing
     */
    SDL_zero (ev);
    ev.type = SDL_USEREVENT;
    ev.user.code = NEXT_IMAGE;
    SDL_PushEvent (&ev);
    return wait_time;
}

int
timer_running (void)
{
    return timer_id != 0;
}

void
timer_toggle (void)
{
    if (timer_id == 0)
	timer_start (wait_time);
    else
	timer_stop ();
    return;
}

void
timer_stop (void)
{
    if (timer_id != 0)
	if (SDL_RemoveTimer (timer_id) == SDL_FALSE)
	    oops ("SDL_RemoveTimer() failed\n");
    timer_id = 0;
    return;
}

void
timer_start (int MILLIS)
{
    wait_time = MILLIS;
    if (timer_id == 0)
	timer_id =
	    SDL_AddTimer (wait_time, timer_stub, NULL);
    return;
}
