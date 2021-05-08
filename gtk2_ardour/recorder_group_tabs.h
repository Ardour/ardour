/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_recorder_group_tabs_h__
#define __gtk_ardour_recorder_group_tabs_h__

#include "group_tabs.h"

class RecorderUI;

class RecorderGroupTabs : public GroupTabs
{
public:
	RecorderGroupTabs (RecorderUI*);

private:
	std::list<Tab>    compute_tabs () const;
	void              draw_tab (cairo_t*, Tab const&);
	ARDOUR::RouteList routes_for_tab (Tab const*) const;
	ARDOUR::RouteList selected_routes () const;
	double            primary_coordinate (double, double) const;
	double            extent () const;

	RecorderUI* _recorder;
};

#endif /* __gtk_ardour_recorder_ui_h__ */
