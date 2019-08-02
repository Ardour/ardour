/*
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_route_processor_selection_h__
#define __ardour_gtk_route_processor_selection_h__

#include <vector>
#include "pbd/signals.h"

#include "processor_selection.h"
#include "route_ui_selection.h"

namespace ARDOUR {
	class SessionHandlePtr;
}

class AxisViewProvider;

class RouteProcessorSelection : public ProcessorSelection
{
public:
	AxisViewSelection  axes;

	RouteProcessorSelection (ARDOUR::SessionHandlePtr&, AxisViewProvider&);

	void clear ();
	bool empty();

	void set (AxisView*);
	void add (AxisView*, bool with_groups = false);
	void remove (AxisView*, bool with_groups = false);
	bool selected (AxisView*);

	void clear_routes ();

	void presentation_info_changed (PBD::PropertyChange const & what_changed);

private:
	ARDOUR::SessionHandlePtr& shp;
	AxisViewProvider& avp;
	void removed (AxisView*);
	std::list<AxisView*> add_grouped_tracks (AxisView*) const;

	RouteProcessorSelection& operator= (const RouteProcessorSelection& other);
	RouteProcessorSelection (RouteProcessorSelection const&);
};

bool operator==(const RouteProcessorSelection& a, const RouteProcessorSelection& b);

#endif /* __ardour_gtk_route_processor_selection_h__ */
