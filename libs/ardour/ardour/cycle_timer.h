/*
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __ardour_cycle_timer_h__
#define __ardour_cycle_timer_h__

#include <atomic>
#include <string>
#include <iostream>
#include <cstdlib>

#include "ardour/libardour_visibility.h"
#include "ardour/cycles.h"
#include "ardour/debug.h"

float get_mhz ();

class LIBARDOUR_API CycleTimer {
  private:
	static float cycles_per_usec;
#ifndef NDEBUG
	cycles_t _entry;
	cycles_t _exit;
	std::string _name;
#endif

  public:
	CycleTimer(const std::string& name) {
#ifndef NDEBUG
		if (DEBUG_ENABLED (PBD::DEBUG::CycleTimers)) {
			_name = name;
			if (cycles_per_usec == 0) {
				cycles_per_usec = get_mhz ();
			}
			_entry = get_cycles();
		}
#else
		(void) name;
#endif
	}

	~CycleTimer() {
#ifndef NDEBUG
		if (DEBUG_ENABLED (PBD::DEBUG::CycleTimers)) {
			_exit = get_cycles();
			std::cerr << _name << ": " << (float) (_exit - _entry) / cycles_per_usec << " (" <<  _entry << ", " << _exit << ')' << std::endl;
		}
#endif
	}
};

class LIBARDOUR_API StoringTimer
{
public:
	StoringTimer ();
	void ref ();
	void check (char const * const what);
	void dump (std::ostream&);

	static void dump_all (std::string const &);
	static StoringTimer* thread_st();
	static void dump_all ();

  private:
	cycles_t _current_ref;

	char const * const thread;
	char const ** _what;
	cycles_t* _value;
	cycles_t* _ref;
	int _points;

	static int _max_points;
	static StoringTimer* all_timers[2048]; /* size relates to thread count */
	static std::atomic<int> st_cnt;
	thread_local static int st_index;
};

#ifdef PT_TIMING
#define PT_TIMING_REF StoringTimer::thread_st()->ref();
#define PT_TIMING_CHECK(w) StoringTimer::thread_st()->check(w);
#endif

#ifndef PT_TIMING
#define PT_TIMING_REF
#define PT_TIMING_CHECK(w)
#endif

#endif /* __ardour_cycle_timer_h__ */
