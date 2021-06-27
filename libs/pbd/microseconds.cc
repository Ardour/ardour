/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2015 John Emmas <john@creativepost.co.uk>
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

#include <time.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#include "pbd/error.h"
#include "pbd/microseconds.h"
#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
static double timer_rate_usecs = 0.0;
#endif

#ifdef __MACH__
#include <mach/mach_time.h>

#ifndef CLOCK_REALTIME
#define MACH_NEED_MICROSECONDS_TIMEBASE

static mach_timebase_info_data_t timebase;

/* Thanks Apple for not implementing this basic SUSv2, POSIX.1-2001 function
 */
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0

int
clock_gettime (int /*clk_id*/, struct timespec* t)
{
	uint64_t time;
	time            = mach_absolute_time ();
	double nseconds = ((double)time * (double)timebase.numer) / ((double)timebase.denom);
	double seconds  = ((double)time * (double)timebase.numer) / ((double)timebase.denom * 1e9);
	t->tv_sec       = seconds;
	t->tv_nsec      = nseconds;
	return 0;
}

#endif /* CLOCK_REALTIME */
#endif /* __MACH__ */

void
PBD::microsecond_timer_init ()
{
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart < 1) {
		info << X_("Failed to determine frequency of QPC\n") << endmsg;
		timer_rate_usecs = 0;
	} else {
		timer_rate_usecs = 1000000.0 / freq.QuadPart;
	}
#endif
#ifdef MACH_NEED_MICROSECONDS_TIMEBASE
	mach_timebase_info (&timebase);
#endif
}

/** Return a monotonic value for the number of microseconds that have elapsed
 * since an arbitrary zero origin.
 */
PBD::microseconds_t
PBD::get_microseconds ()
{
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER time;

	if (timer_rate_usecs) {
		if (QueryPerformanceCounter (&time)) {
			return (microseconds_t) (time.QuadPart * timer_rate_usecs);
		}
	}

	return (microseconds_t) 0;
#else
	struct timespec ts;
	if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0) {
		/* EEEK! */
		return 0;
	}
	return (microseconds_t)ts.tv_sec * 1000000 + (ts.tv_nsec / 1000);
#endif
}
