/*
    Copyright (C) 2000-2011 Paul Davis

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

#ifndef __libardour_region_sorters_h__
#define __libardour_region_sorters_h__

#include "ardour/region.h"

namespace ARDOUR {

struct RegionSortByPosition {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->position() < b->position();
	}
};

struct RegionSortByLastLayerOp {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->last_layer_op() < b->last_layer_op();
	}
};

struct RegionSortByLayer {
	bool operator() (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->layer() < b->layer();
	}
};

struct RegionSortByLayerWithPending {
	bool operator () (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {

		double p = a->layer ();
		if (a->pending_explicit_relayer()) {
			p += 0.5;
		}

		double q = b->layer ();
		if (b->pending_explicit_relayer()) {
			q += 0.5;
		}

		return p < q;
	}
};

} // namespace

#endif /* __libardour_region_sorters_h__ */
