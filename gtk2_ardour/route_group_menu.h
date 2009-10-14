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

#include "ardour/route_group.h"

namespace ARDOUR {
	class Session;
}

class RouteGroupMenu : public Gtk::Menu
{
public:
	RouteGroupMenu (ARDOUR::Session &, ARDOUR::RouteGroup::Property);

	void rebuild (ARDOUR::RouteGroup *);

	sigc::signal<void, ARDOUR::RouteGroup*> GroupSelected;

private:
	void add_item (ARDOUR::RouteGroup *, ARDOUR::RouteGroup *, Gtk::RadioMenuItem::Group*);
	void new_group ();
	void set_group (ARDOUR::RouteGroup *);

	ARDOUR::Session& _session;
	ARDOUR::RouteGroup::Property _default_properties;
};
