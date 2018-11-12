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

#include "ardour/stripable.h"

/* This is used to keep numerical tree-order in sync
 * with Stripable ordering (mixer_ui.cc editor_routes.cc)
 */

struct TreeOrderKey {
    uint32_t old_display_order;
		boost::shared_ptr<ARDOUR::Stripable> stripable;

	TreeOrderKey (uint32_t ok, boost::shared_ptr<ARDOUR::Stripable> s)
		: old_display_order (ok)
		, stripable (s)
	{}
};

typedef std::vector<TreeOrderKey> TreeOrderKeys;

struct TreeOrderKeySorter
{
	bool operator() (const TreeOrderKey& ok_a, const TreeOrderKey& ok_b)
	{
		boost::shared_ptr<ARDOUR::Stripable> const& a = ok_a.stripable;
		boost::shared_ptr<ARDOUR::Stripable> const& b = ok_b.stripable;
		return ARDOUR::Stripable::Sorter () (a, b);
	}
};

#endif /* __gtk2_ardour_route_sorter_h__ */
