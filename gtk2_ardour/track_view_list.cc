/*
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include "track_view_list.h"
#include "route_time_axis.h"

using namespace std;

TrackViewList::TrackViewList (list<TimeAxisView*> const & t)
	: list<TimeAxisView*> (t)
{
}

TrackViewList::~TrackViewList ()
{
}

bool
TrackViewList::contains (TimeAxisView const * t) const
{
	return find (begin(), end(), t) != end();
}

TrackViewList
TrackViewList::add (TrackViewList const & t)
{
	TrackViewList added;

	for (TrackViewList::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (!contains (*i)) {
			added.push_back (*i);
			push_back (*i);
		}
	}

	return added;
}

ARDOUR::RouteList
TrackViewList::routelist () const
{
	ARDOUR::RouteList rl;
	for (TrackViewList::const_iterator i = begin (); i != end (); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtv) {
			rl.push_back (rtv->route ());
		}
	}
	return rl;
}
