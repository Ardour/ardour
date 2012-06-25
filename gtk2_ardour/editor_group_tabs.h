/*
    Copyright (C) 2009 Paul Davis

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

#include <gtkmm/menu.h>
#include "group_tabs.h"

class Editor;

class EditorGroupTabs : public GroupTabs, public EditorComponent
{
public:
	EditorGroupTabs (Editor *);

private:
	std::list<Tab> compute_tabs () const;
	void draw_tab (cairo_t *, Tab const &) const;
	double primary_coordinate (double, double) const;
	ARDOUR::RouteList routes_for_tab (Tab const *) const;
	double extent () const {
		return get_height();
	}
	void add_menu_items (Gtk::Menu *, ARDOUR::RouteGroup *);
	PBD::PropertyList default_properties () const;
        ARDOUR::RouteSortOrderKey order_key () const;
	ARDOUR::RouteList selected_routes () const;
	void sync_order_keys ();
};
