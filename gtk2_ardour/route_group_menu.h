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

#ifndef __ardour_gtk_route_group_menu_h__
#define __ardour_gtk_route_group_menu_h__

#include "ardour/route_group.h"
#include "ardour/session_handle.h"


class RouteGroupMenu : public Gtk::Menu, public ARDOUR::SessionHandlePtr
{
public:
	RouteGroupMenu (ARDOUR::Session*, PBD::PropertyList*);
	~RouteGroupMenu();

	void rebuild (ARDOUR::RouteGroup *);

	sigc::signal<void, ARDOUR::RouteGroup*> GroupSelected;

  private:
	void add_item (ARDOUR::RouteGroup *, ARDOUR::RouteGroup *, Gtk::RadioMenuItem::Group*);
	void new_group ();
	void set_group (ARDOUR::RouteGroup *);

	PBD::PropertyList* _default_properties;
	bool _inhibit_group_selected;
	ARDOUR::RouteGroup* _selected_route_group;
};

#endif /* __ardour_gtk_route_group_menu_h__ */
