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
#include "cairo_widget.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
}

class Editor;

class GroupTabs : public CairoWidget
{
public:
	GroupTabs ();

	void set_session (ARDOUR::Session *);

private:
	virtual ARDOUR::RouteGroup* click_to_route_group (GdkEventButton* ev) = 0;
	virtual void render (cairo_t *) = 0;
	
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);

	void edit_group (ARDOUR::RouteGroup *);
	void remove_group (ARDOUR::RouteGroup *);

	ARDOUR::Session* _session;
	Gtk::Menu* _menu;
};
