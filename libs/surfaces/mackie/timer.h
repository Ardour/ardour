/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef timer_h
#define timer_h

#ifdef _WIN32
#include "windows.h"
#else
#include <sys/time.h>
#endif

namespace ArdourSurface {

namespace Mackie
{

/**
	millisecond timer class.
*/
class Timer
{
public:

	/**
		start the timer running if true, or just create the
		object if false.
	*/
	Timer( bool shouldStart = true )
	{
		if ( shouldStart )
			start();
	}

	/**
		Start the timer running. Return the current timestamp, in milliseconds
	*/
	unsigned long start()
	{
		_start = g_get_monotonic_time();
		return _start / 1000;
	}

	/**
		returns the number of milliseconds since start
		also stops the timer running
	*/
	unsigned long stop()
	{
		_stop = g_get_monotonic_time();
		return elapsed();
	}

	/**
		returns the number of milliseconds since start
	*/
	unsigned long elapsed() const
	{
		if ( running )
		{
			uint64_t now = g_get_monotonic_time();
			return (now - _start) / 1000;
		}
		else
		{
			return (_stop - _start) / 1000;
		}
	}

	/**
		Call stop and then start. Return the value from stop.
	*/
	unsigned long restart()
	{
		unsigned long retval = stop();
		start();
		return retval;
	}

private:
	uint64_t _start;
	uint64_t _stop;
	bool running;
};

} // Mackie namespace
} // ArdourSurface namespace

#endif
