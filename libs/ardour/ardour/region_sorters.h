/*
 * Copyright (C) 2011-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __libardour_region_sorters_h__
#define __libardour_region_sorters_h__

#include "ardour/region.h"

namespace ARDOUR {

struct LIBARDOUR_API RegionSortByPosition {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->position() < b->position();
	}
};

struct LIBARDOUR_API RegionSortByLayer {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->layer() < b->layer();
	}
};

/* sort by RegionSortByLayerAndPosition()
 * is equivalent to
 * stable_sort by RegionSortByPosition();
 * stable_sort by RegionSortByLayer();
 */
struct LIBARDOUR_API RegionSortByLayerAndPosition {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return
			   (a->layer() < b->layer()  && a->position() < b->position())
			|| (a->layer() == b->layer() && a->position() < b->position());
	}
};

} // namespace

#endif /* __libardour_region_sorters_h__ */
