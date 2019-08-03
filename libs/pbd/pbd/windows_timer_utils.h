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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PBD_WINDOWS_TIMER_UTILS_H
#define PBD_WINDOWS_TIMER_UTILS_H

#include <stdint.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

namespace MMTIMERS {

/**
 * Get the minimum Multimedia Timer resolution as supported by the system
 * @return true if getting min timer resolution was not successful
 */
bool LIBPBD_API get_min_resolution (uint32_t& timer_resolution_ms);

/**
 * Set the minimum Multimedia Timer resolution as supported by the system
 * @return true if setting min timer resolution was successful
 */
bool LIBPBD_API set_min_resolution ();

/**
 * Set current Multimedia Timer resolution. If the timer resolution has already
 * been set then reset_resolution() must be called before set_resolution will
 * succeed.
 * @return true if setting the timer value was successful, false if setting the
 * timer resolution failed or the resolution has already been set.
 */
bool LIBPBD_API set_resolution(uint32_t timer_resolution_ms);

/**
 * Reset Multimedia Timer resolution. In my testing, if the timer resolution is
 * set below the default, then resetting the resolution will not reset the
 * timer resolution back to 15ms. At least it does not reset immediately
 * even after calling Sleep.
 * @return true if setting the timer value was successful
 */
bool LIBPBD_API reset_resolution();

} // namespace MMTIMERS

namespace QPC {

/**
 * Initialize the QPC timer, must be called before QPC::get_microseconds will
 * return a valid value.
 * @return true if QPC timer is usable, use check_timer_valid to try to check
 * if it is monotonic.
 */
bool LIBPBD_API initialize ();

/**
 * @return true if QueryPerformanceCounter is usable as a timer source
 * This should always return true for systems > XP as those versions of windows
 * have there own tests to check timer validity and will select an appropriate
 * timer source. This check is not conclusive and there are probably conditions
 * under which this check will return true but the timer is not monotonic.
 */
bool LIBPBD_API check_timer_valid ();

/**
 * @return the value of the performance counter converted to microseconds
 *
 * If initialize returns true then get_microseconds will always return a
 * positive value. If QPC is not supported(OS < XP) then -1 is returned but the
 * MS docs say that this won't occur for systems >= XP.
 */
int64_t LIBPBD_API get_microseconds ();

} // namespace QPC

/**
 * The highest resolution timer source provided by the system. On Vista and
 * above this is the value returned by QueryPerformanceCounter(QPC). On XP,
 * this will QPC if supported or otherwise g_get_monotonic_time will be used.
 *
 * @return A timer value in microseconds or -1 in the event that the reading
 * the timer source fails.
 */
int64_t LIBPBD_API get_microseconds ();

} // namespace PBD

#endif // PBD_WINDOWS_TIMER_UTILS_H
