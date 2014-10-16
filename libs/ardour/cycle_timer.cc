/*
    Copyright (C) 2002 Andrew Morton

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

#include <cstdio>
#include <fstream>
#include "pbd/error.h"
#include "ardour/cycle_timer.h"

#include "ardour/libardour_visibility.h"

#include "i18n.h"

using namespace std;
using namespace PBD;

float CycleTimer::cycles_per_usec = 0;

float
get_mhz()
{
	FILE *f;

	if ((f = fopen("/proc/cpuinfo", "r")) == 0) {
		fatal << _("CycleTimer::get_mhz(): can't open /proc/cpuinfo") << endmsg;
		/*NOTREACHED*/
		return 0.0f;
	}

	while (true) {

		float mhz;
		int ret;
		char buf[1000];

		if (fgets (buf, sizeof(buf), f) == 0) {
			fatal << _("CycleTimer::get_mhz(): cannot locate cpu MHz in /proc/cpuinfo") << endmsg;
			/*NOTREACHED*/
			return 0.0f;
		}

#ifdef __powerpc__

		int   imhz;

		/* why can't the PPC crew standardize their /proc/cpuinfo format ? */
		ret = sscanf (buf, "clock\t: %dMHz", &imhz);
		mhz = (float) imhz;

#else /* XXX don't assume its x86 just because its not power pc */
		ret = sscanf (buf, "cpu MHz         : %f", &mhz);

#endif
		if (ret == 1) {
			fclose(f);
			return mhz;
		}
	}

	fatal << _("cannot locate cpu MHz in /proc/cpuinfo") << endmsg;
	/*NOTREACHED*/
	return 0.0f;
}

StoringTimer::StoringTimer (int N)
{
	_point = new int[N];
	_value = new cycles_t[N];
	_ref = new cycles_t[N];
	_max_points = N;
	_points = 0;
}
	

void
StoringTimer::dump (string const & file)
{
	ofstream f (file.c_str ());

	f << min (_points, _max_points) << "\n";
	f << get_mhz () << "\n";
	for (int i = 0; i < min (_points, _max_points); ++i) {
		f << _point[i] << " " << _ref[i] << " " << _value[i] << "\n";
	}
}

void
StoringTimer::ref ()
{
	_current_ref = get_cycles ();
}

void
StoringTimer::check (int p)
{
	if (_points == _max_points) {
		++_points;
		return;
	} else if (_points > _max_points) {
		return;
	}
	
	_point[_points] = p;
	_value[_points] = get_cycles ();
	_ref[_points] = _current_ref;
	
	++_points;
}

#ifdef PT_TIMING
StoringTimer ST (64 * 1024);
#endif


