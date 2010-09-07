/*
    Copyright (C) 2000-2009 Paul Davis

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
#include "ardour/route_group.h"
#include "track_selection.h"
#include "time_axis_view.h"
#include "public_editor.h"

using namespace std;

TrackSelection::TrackSelection (PublicEditor const * e, TrackViewList const &t)
	: TrackViewList (t)
	, _editor (e)
{
}

TrackSelection::~TrackSelection ()
{
}

TrackViewList
TrackSelection::add (TrackViewList const & t)
{
	TrackViewList added = TrackViewList::add (t);

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {

		/* select anything in the same select-enabled route group */
		ARDOUR::RouteGroup* rg = (*i)->route_group ();
		if (rg && rg->is_active() && rg->is_select ()) {
			TrackViewList tr = _editor->axis_views_from_routes (rg->route_list ());
			for (TrackViewList::iterator j = tr.begin(); j != tr.end(); ++j) {
				if (!contains (*j)) {
					added.push_back (*j);
					push_back (*j);
				}
			}
		}
	}

	return added;
}

