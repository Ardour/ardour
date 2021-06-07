/*
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#include <cstdio>
#include <fstream>
#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "ardour/cycle_timer.h"

#include "ardour/libardour_visibility.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

float CycleTimer::cycles_per_usec = 0;

float
get_mhz()
{
	FILE *f;

	if ((f = fopen("/proc/cpuinfo", "r")) == 0) {
		fatal << _("CycleTimer::get_mhz(): can't open /proc/cpuinfo") << endmsg;
		abort(); /*NOTREACHED*/
		return 0.0f;
	}

	while (true) {

		float mhz;
		int ret;
		char buf[1000];

		if (fgets (buf, sizeof(buf), f) == 0) {
			fatal << _("CycleTimer::get_mhz(): cannot locate cpu MHz in /proc/cpuinfo") << endmsg;
			abort(); /*NOTREACHED*/
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
	abort(); /*NOTREACHED*/
	return 0.0f;
}

int StoringTimer::_max_points = 64 * 1024;
StoringTimer* StoringTimer::all_timers[2048];
std::atomic<int> StoringTimer::st_cnt;

thread_local int StoringTimer::st_index = st_cnt.fetch_add (1);

StoringTimer::StoringTimer ()
	: thread (strdup (pthread_name()))
{
	_what = new char const *[_max_points];
	_value = new cycles_t[_max_points];
	_ref = new cycles_t[_max_points] ;
	_points = 0;
}

void
StoringTimer::dump_all (string const & file)
{
	ofstream f (file.c_str ());

	f << get_mhz () << "\n";
	f << "There were " << st_cnt.load() << " thread timers\n";

	for (size_t i = 0; i < (sizeof (all_timers) / sizeof (all_timers[0])); ++i) {
		if (all_timers[i]) {
			all_timers[i]->dump (f);
		}
	}
}

void
StoringTimer::dump (std::ostream& f)
{
	f << thread << ' ' << _points << "\n";

	for (int i = 0; i < min (_points, _max_points); ++i) {
		f << '\t' << _what[i] << " " << _ref[i] << " " << _value[i] << " delta " << _value[i] - _ref[i] << "\n";
	}
}

void
StoringTimer::ref ()
{
	_current_ref = get_cycles ();
}

void
StoringTimer::check (char const * const what)
{
	cerr << pthread_name() << " @ " << pthread_self() << ' ' << thread << " check " << what << " @ " << _points << endl;

	if (_points == _max_points) {
		++_points;
		return;
	} else if (_points > _max_points) {
		return;
	}

	_what[_points] = what;
	_value[_points] = get_cycles ();
	_ref[_points] = _current_ref;

	++_points;
}

StoringTimer*
StoringTimer::thread_st()
{
	if (all_timers[st_index] == 0) {
		all_timers[st_index] = new StoringTimer;
	}
	return all_timers[st_index];
}


