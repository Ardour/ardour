/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_editor_group_tabs_h__
#define __gtk_ardour_editor_group_tabs_h__

#include <gtkmm/menu.h>
#include "group_tabs.h"

class Editor;

class EditorGroupTabs : public GroupTabs, public EditorComponent
{
public:
	EditorGroupTabs (Editor *);

private:
	std::list<Tab> compute_tabs () const;
	void draw_tab (cairo_t *, Tab const &);
	double primary_coordinate (double, double) const;
	ARDOUR::RouteList routes_for_tab (Tab const *) const;
	double extent () const {
		return get_height();
	}
	void add_menu_items (Gtk::Menu *, ARDOUR::RouteGroup *);
	ARDOUR::RouteList selected_routes () const;
};

#endif // __gtk_ardour_editor_group_tabs_h__
