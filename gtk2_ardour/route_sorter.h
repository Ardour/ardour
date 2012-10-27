/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __gtk2_ardour_route_sorter_h__
#define __gtk2_ardour_route_sorter_h__

#include <boost/shared_ptr.hpp>
#include <stdint.h>
#include <vector>

namespace ARDOUR {
	class Route;
}

struct RoutePlusOrderKey {
    boost::shared_ptr<ARDOUR::Route> route; /* we don't really need this, but its handy to keep around */
    uint32_t old_display_order;
    uint32_t new_display_order;

    RoutePlusOrderKey (boost::shared_ptr<ARDOUR::Route> r, uint32_t ok, uint32_t nk)
	    : route (r)
	    , old_display_order (ok)
	    , new_display_order (nk) {}
};
	
typedef std::vector<RoutePlusOrderKey> OrderKeySortedRoutes;

struct SortByNewDisplayOrder {
    bool operator() (const RoutePlusOrderKey& a, const RoutePlusOrderKey& b) {
	    return a.new_display_order < b.new_display_order;
    }
};

#endif /* __gtk2_ardour_route_sorter_h__ */
