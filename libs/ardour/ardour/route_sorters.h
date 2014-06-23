/*
    Copyright (C) 2000-2014 Paul Davis

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

#ifndef __libardour_route_sorters_h__
#define __libardour_route_sorters_h__

#include "ardour/route.h"

namespace ARDOUR {

struct SignalOrderRouteSorter {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master() || a->is_monitor()) {
			/* "a" is a special route (master, monitor, etc), and comes
			 * last in the mixer ordering
			 */
			return false;
		} else if (b->is_master() || b->is_monitor()) {
			/* everything comes before b */
			return true;
		}
		return a->order_key () < b->order_key ();
	}
};

} // namespace

#endif /* __libardour_route_sorters_h__ */
