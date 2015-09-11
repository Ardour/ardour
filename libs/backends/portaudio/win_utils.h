/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef WIN_UTILS_H
#define WIN_UTILS_H

#include <stdint.h>

namespace utils {

bool set_min_timer_resolution ();

bool reset_timer_resolution ();

/** The highest resolution timer source provided by the system. On Vista and
 * above this is the value returned by QueryPerformanceCounter(QPC). On XP,
 * this will QPC if supported or otherwise g_get_monotonic_time will be used.
 *
 * @return A timer value in microseconds or -1 in the event that the reading
 * the timer source fails, but the MS docs say that this won't occur for
 * systems >= XP
 */
int64_t get_microseconds ();

}

#endif // WIN_UTILS_H
