/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2009 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk_selection_templates_h__
#define __ardour_gtk_selection_templates_h__

/* these inlines require knowledge of Region and Route classes,
   and so they are in a separate header file from selection.h to
   avoid multiplying dependencies.
*/

#include "ardour/region.h"

#include "selection.h"
#include "region_view.h"
#include "midi_region_view.h"

inline void
Selection::foreach_region (void (ARDOUR::Region::*method)(void)) {
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		ARDOUR::Region* region = (*i)->region().get();
		(region->*(method))();
	}
}

inline void
Selection::foreach_regionview (void (RegionView::*method)(void)) {
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		((*i)->*(method))();
	}
}

inline void
Selection::foreach_midi_regionview (void (MidiRegionView::*method)(void)) {
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			(mrv->*(method))();
		}
	}
}

template<class A> inline void
Selection::foreach_region (void (ARDOUR::Region::*method)(A), A arg) {
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		ARDOUR::Region* region = (*i)->region().get();
		(region->*(method))(arg);
	}
}

#endif /* __ardour_gtk_selection_templates_h__ */
