
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
 ****************************************************************************/

#ifndef TIMER_H
#define TIMER_H

#define NEXT_IMAGE 0x02

/* Default slideshow interval, milliseconds (2.5s), shared with vp.c's -s
 * option default. */
#define DEFAULT_SLIDESHOW_MS 2500

void timer_toggle (void);
void timer_stop (void);
void timer_start (int millis);
int timer_running (void);

#endif
