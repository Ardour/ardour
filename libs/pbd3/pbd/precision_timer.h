/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __precision_timer_h__
#define __precision_timer_h__

#include <pbd/cycles.h>

typedef cycles_t precision_time_t;

class PrecisionTimer {
  public:
	PrecisionTimer ();

	/* returns current time in microseconds since
	   the time base was created (which may be
	   the same as when the PrecisionTimer was
	   created or it may not).
	*/
	
#ifdef PBD_HAVE_CYCLE_COUNTER

	precision_time_t current () {
		return get_cycles() / cycles_per_usec;
	}

#else /* !HAVE_CYCLE_COUNTER */

	precision_time_t current () {
		struct timeval now;
		gettimeofday (&now, 0);
		return (precision_time_t) ((now.tv_sec * 1000000) + now.tv_usec);
	}
	
#endif /* HAVE_CYCLE_COUNTER */
	
  private:
	int get_mhz();
	static precision_time_t cycles_per_usec;
};

#endif // __precision_timer.h


