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

#ifndef PBD_WINDOWS_TIMER_UTILS_H
#define PBD_WINDOWS_TIMER_UTILS_H

#include <stdint.h>

namespace PBD {

namespace MMTIMERS {

/**
 * Set the minimum Multimedia Timer resolution as supported by the system
 * @return true if min timer resolution was successfully set
 *
 * Call reset_resolution to restore old timer resolution
 */
bool set_min_resolution ();

/**
 * Get current Multimedia Timer resolution
 * @return true if getting the timer value was successful
 */
bool get_resolution(uint32_t& timer_resolution_us);

/**
 * Set current Multimedia Timer resolution
 * @return true if setting the timer value was successful
 */
bool set_resolution(uint32_t timer_resolution_us);

/**
 * Reset the Multimedia Timer back to what it was originally before
 * setting the timer resolution.
 */
bool reset_resolution ();

} // namespace MMTIMERS

namespace QPC {

/**
 * @return true if QueryPerformanceCounter is usable as a timer source
 */
bool get_timer_valid ();

/**
 * @return the value of the performance counter converted to microseconds
 *
 * If get_counter_valid returns true then get_microseconds will always
 * return a positive value. If QPC is not supported(OS < XP) then -1 is
 * returned but the MS docs say that this won't occur for systems >= XP.
 */
int64_t get_microseconds ();

} // namespace QPC

/**
 * The highest resolution timer source provided by the system. On Vista and
 * above this is the value returned by QueryPerformanceCounter(QPC). On XP,
 * this will QPC if supported or otherwise g_get_monotonic_time will be used.
 *
 * @return A timer value in microseconds or -1 in the event that the reading
 * the timer source fails.
 */
int64_t get_microseconds ();

} // namespace PBD

#endif // PBD_WINDOWS_TIMER_UTILS_H
