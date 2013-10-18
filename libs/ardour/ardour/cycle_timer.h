/*
    Copyright (C) 2002 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_cycle_timer_h__
#define __ardour_cycle_timer_h__

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
		if (PBD::debug_bits & PBD::DEBUG::CycleTimers) {
			_name = name;
			if (cycles_per_usec == 0) {
				cycles_per_usec = get_mhz ();
			}
			_entry = get_cycles();
		}
#endif
	}

	~CycleTimer() {
#ifndef NDEBUG
		if (PBD::debug_bits & PBD::DEBUG::CycleTimers) {
			_exit = get_cycles();
			std::cerr << _name << ": " << (float) (_exit - _entry) / cycles_per_usec << " (" <<  _entry << ", " << _exit << ')' << std::endl;
		}
#endif
	}
};

class LIBARDOUR_API StoringTimer
{
public:
	StoringTimer (int);
	void ref ();
	void check (int);
	void dump (std::string const &);

private:
	cycles_t _current_ref;
	int* _point;
	cycles_t* _value;
	cycles_t* _ref;
	int _points;
	int _max_points;
};

#ifdef PT_TIMING
extern StoringTimer ST;
#define PT_TIMING_REF ST.ref();
#define PT_TIMING_CHECK(x) ST.check(x);
#endif

#ifndef PT_TIMING
#define PT_TIMING_REF
#define PT_TIMING_CHECK(x)
#endif

#endif /* __ardour_cycle_timer_h__ */
