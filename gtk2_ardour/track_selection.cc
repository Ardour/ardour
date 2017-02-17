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
#include "control_protocol/control_protocol.h"

#include "track_selection.h"
#include "time_axis_view.h"
#include "public_editor.h"
#include "vca_time_axis.h"

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
	TrackViewList added;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (dynamic_cast<VCATimeAxisView*> (*i)) {
			continue;
		}

		/* select anything in the same select-enabled route group */
		ARDOUR::RouteGroup* rg = (*i)->route_group ();

		if (rg && rg->is_active() && rg->is_select ()) {

			TrackViewList tr = _editor->axis_views_from_routes (rg->route_list ());

			for (TrackViewList::iterator j = tr.begin(); j != tr.end(); ++j) {

				/* Do not add the trackview passed in as an
				 * argument, because we want that to be on the
				 * end of the list.
				 */

				if (*j != *i) {
					if (!contains (*j)) {
						added.push_back (*j);
						push_back (*j);
					}
				}
			}
		}

		/* now add the the trackview's passed in as actual arguments */

		if (!contains (*i)) {
			added.push_back (*i);
			push_back (*i);
		}
	}


	return added;
}
