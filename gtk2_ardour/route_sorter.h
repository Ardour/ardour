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

struct OrderKeys {
    uint32_t old_display_order;
    uint32_t new_display_order;
    uint32_t compare_order;

	OrderKeys (uint32_t ok, boost::shared_ptr<ARDOUR::Stripable> s, uint32_t cmp_max)
		: old_display_order (ok)
	{
		new_display_order = s->presentation_info().order();
		compare_order = new_display_order;

		if (s->presentation_info().flags () & ARDOUR::PresentationInfo::VCA) {
			compare_order += 2 * cmp_max;
		}
#ifdef MIXBUS
		if (s->presentation_info().flags () & ARDOUR::PresentationInfo::Mixbus || s->mixbus()) {
			compare_order += cmp_max;
		}
		if (s->presentation_info().flags () & ARDOUR::PresentationInfo::MasterOut) {
			compare_order += 3 * cmp_max;
		}
#endif
	}
};

typedef std::vector<OrderKeys> OrderingKeys;

struct SortByNewDisplayOrder {
    bool operator() (const OrderKeys& a, const OrderKeys& b) {
	    return a.compare_order < b.compare_order;
    }
};

struct StripablePresentationInfoSorter {
	bool operator() (boost::shared_ptr<ARDOUR::Stripable> a, boost::shared_ptr<ARDOUR::Stripable> b) {
		return a->presentation_info().order () < b->presentation_info().order ();
	}
};

#endif /* __gtk2_ardour_route_sorter_h__ */
