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

#include "ardour/cycles.h"

class CycleTimer {
  private:
	static float cycles_per_usec;
	cycles_t _entry;
	cycles_t _exit;
	std::string _name;
	
  public:
	CycleTimer(std::string name) : _name (name){
		if (cycles_per_usec == 0) {
			cycles_per_usec = get_mhz ();
		}
		_entry = get_cycles();
	}
	~CycleTimer() {
		_exit = get_cycles();
		std::cerr << _name << ": " << (float) (_exit - _entry) / cycles_per_usec << " (" <<  _entry << ", " << _exit << ')' << endl;
	}

	static float get_mhz ();
};

#endif /* __ardour_cycle_timer_h__ */
