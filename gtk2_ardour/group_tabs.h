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

protected:

	struct Tab {
		double from;
		double to;
		Gdk::Color colour;
		ARDOUR::RouteGroup* group;
	};

private:
	virtual std::list<Tab> compute_tabs () const = 0;
	virtual void draw_tab (cairo_t *, Tab const &) const = 0;
	virtual double primary_coordinate (double, double) const = 0;
	virtual void reflect_tabs (std::list<Tab> const &) = 0;
	virtual double extent () const = 0;

	void render (cairo_t *);
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);
	bool on_button_release_event (GdkEventButton *);

	Tab * click_to_tab (double, Tab**, Tab**);
	void edit_group (ARDOUR::RouteGroup *);
	void remove_group (ARDOUR::RouteGroup *);

	ARDOUR::Session* _session;
	Gtk::Menu* _menu;
	std::list<Tab> _tabs;
	Tab* _dragging;
	bool _drag_moved;
	bool _drag_from;
	double _drag_last;
	double _drag_limit;
};
