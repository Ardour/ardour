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

    $Id$
*/

#ifndef __ardour_cycle_timer_h__
#define __ardour_cycle_timer_h__

#include <string>
#include <cstdio>

#include <ardour/cycles.h>

using std::string;

class CycleTimer {
  private:
	static float cycles_per_usec;
	uint32_t long entry;
	uint32_t long exit;
	string _name;
	
  public:
	CycleTimer(string name) : _name (name){
		if (cycles_per_usec == 0) {
			cycles_per_usec = get_mhz ();
		}
		entry = get_cycles();
	}
	~CycleTimer() {
		exit = get_cycles();
		printf ("%s: %.9f usecs (%lu-%lu)\n", _name.c_str(), (float) (exit - entry) / cycles_per_usec, entry, exit);
	}

	static float get_mhz ();
};

#endif /* __ardour_cycle_timer_h__ */
