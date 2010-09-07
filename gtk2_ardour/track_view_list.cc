/*
    Copyright (C) 2009 Paul Davis

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

#include <algorithm>
#include "track_view_list.h"

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
