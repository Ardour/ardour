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

#include "pbd/windows_timer_utils.h"

#include <windows.h>
#include <mmsystem.h>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

#define DEBUG_TIMING(msg) DEBUG_TRACE (PBD::DEBUG::Timing, msg);

namespace {

static
UINT&
timer_resolution ()
{
	static UINT timer_res_ms = 0;
	return timer_res_ms;
}

} // namespace

namespace PBD {

namespace MMTIMERS {

bool
get_min_resolution (uint32_t& min_resolution_ms)
{
	TIMECAPS caps;

	if (timeGetDevCaps (&caps, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		DEBUG_TIMING ("Could not get timer device capabilities.\n");
		return false;
	}

	min_resolution_ms = caps.wPeriodMin;
	return true;
}

bool
set_min_resolution ()
{
	uint32_t min_resolution = 0;

	if (!get_min_resolution (min_resolution)) {
		return false;
	}

	if (!set_resolution (min_resolution)) {
		return false;
	}
	return true;
}

bool
set_resolution (uint32_t timer_resolution_ms)
{
	if (timer_resolution() != 0) {
		DEBUG_TIMING(
		    "Timer resolution must be reset before setting new resolution.\n");
	}

	if (timeBeginPeriod(timer_resolution_ms) != TIMERR_NOERROR) {
		DEBUG_TIMING(
		    string_compose("Could not set timer resolution to %1(ms)\n",
		                   timer_resolution_ms));
		return false;
	}

	timer_resolution() = timer_resolution_ms;

	DEBUG_TIMING (string_compose ("Multimedia timer resolution set to %1(ms)\n",
	                              timer_resolution_ms));
	return true;
}

bool
reset_resolution ()
{
	// You must match calls to timeBegin/EndPeriod with the same resolution
	if (timeEndPeriod(timer_resolution()) != TIMERR_NOERROR) {
		DEBUG_TIMING("Could not reset the Timer resolution.\n");
		return false;
	}
	DEBUG_TIMING("Reset the Timer resolution.\n");
	timer_resolution() = 0;
	return true;
}

} // namespace MMTIMERS

namespace {

static double timer_rate_us = 0.0;

static
bool
test_qpc_validity ()
{
	int64_t last_timer_val = PBD::QPC::get_microseconds ();
	if (last_timer_val < 0) return false;

	for (int i = 0; i < 100000; ++i) {
		int64_t timer_val = PBD::QPC::get_microseconds ();
		if (timer_val < 0) return false;
		// try and test for non-syncronized TSC(AMD K8/etc)
		if (timer_val < last_timer_val) return false;
		last_timer_val = timer_val;
	}
	return true;
}

} // anon namespace

namespace QPC {

bool
check_timer_valid ()
{
	if (!timer_rate_us) {
		return false;
	}
	return test_qpc_validity ();
}

bool
initialize ()
{
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart < 1) {
		info << X_("Failed to determine frequency of QPC\n") << endmsg;
		timer_rate_us = 0;
	} else {
		timer_rate_us = 1000000.0 / freq.QuadPart;
	}
	info << string_compose(X_("QPC timer microseconds per tick: %1\n"),
	                       timer_rate_us) << endmsg;
	return !timer_rate_us;
}

int64_t
get_microseconds ()
{
	LARGE_INTEGER current_val;

	if (timer_rate_us) {
		// MS docs say this will always succeed for systems >= XP but it may
		// not return a monotonic value with non-invariant TSC's etc
		if (QueryPerformanceCounter(&current_val) != 0) {
			return (int64_t)(current_val.QuadPart * timer_rate_us);
		}
	}
	DEBUG_TIMING ("Could not get QPC timer\n");
	return -1;
}

} // namespace QPC

int64_t
get_microseconds ()
{
	if (timer_rate_us) {
		return QPC::get_microseconds ();
	}
	// For XP systems that don't support a high-res performance counter
	return g_get_monotonic_time ();
}

} // namespace PBD
