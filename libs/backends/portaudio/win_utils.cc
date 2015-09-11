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

#include "win_utils.h"

#include <windows.h>
#include <mmsystem.h>

#include "pbd/compose.h"

#include "debug.h"

namespace {

bool&
qpc_frequency_success ()
{
	static bool success = false;
	return success;
}

LARGE_INTEGER
qpc_frequency ()
{
	LARGE_INTEGER freq;
	if (QueryPerformanceFrequency(&freq) == 0) {
		DEBUG_TIMING ("Failed to determine frequency of QPC\n");
		qpc_frequency_success() = false;
	} else {
		qpc_frequency_success() = true;
	}

	return freq;
}

UINT&
old_timer_resolution ()
{
	static UINT timer_res_ms = 0;
	return timer_res_ms;
}

} // anon namespace

namespace utils {

bool
set_min_timer_resolution ()
{
	TIMECAPS caps;

	if (timeGetDevCaps (&caps, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		DEBUG_TIMING ("Could not get timer device capabilities.\n");
		return false;
	} else {
		old_timer_resolution () = caps.wPeriodMin;
		if (timeBeginPeriod (caps.wPeriodMin) != TIMERR_NOERROR) {
			DEBUG_TIMING (string_compose (
			    "Could not set minimum timer resolution to %1(ms)\n", caps.wPeriodMin));
			return false;
		}
	}

	DEBUG_TIMING (string_compose ("Multimedia timer resolution set to %1(ms)\n",
	                              caps.wPeriodMin));

	return true;
}

bool
reset_timer_resolution ()
{
	if (old_timer_resolution ()) {
		if (timeEndPeriod (old_timer_resolution ()) != TIMERR_NOERROR) {
			DEBUG_TIMING ("Could not reset timer resolution.\n");
			return false;
		}
	}

	DEBUG_TIMING (string_compose ("Multimedia timer resolution set to %1(ms)\n",
	                              old_timer_resolution ()));

	return true;
}

int64_t
get_microseconds ()
{
	static LARGE_INTEGER frequency = qpc_frequency ();
	LARGE_INTEGER current_val;

	if (qpc_frequency_success()) {

		// MS docs say this will always succeed for systems >= XP but it may
		// not return a monotonic value with non-invariant TSC's etc
		if (QueryPerformanceCounter(&current_val) != 0) {
			return (int64_t)(((double)current_val.QuadPart) /
			                 ((double)frequency.QuadPart) * 1000000.0);
		} else {
			DEBUG_TIMING ("Could not get QPC timer\n");
		}
		return -1;
	}
	// For XP systems that don't support a high-res performance counter
	return g_get_monotonic_time ();
}

} // namespace utils
