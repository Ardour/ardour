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

#ifndef __gtk_ardour_route_group_dialog_h__
#define __gtk_ardour_route_group_dialog_h__

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/colorbutton.h>

#include "ardour_dialog.h"

class RouteGroupDialog : public ArdourDialog
{
public:
	RouteGroupDialog (ARDOUR::RouteGroup *, bool);

	bool do_run ();

private:
	ARDOUR::RouteGroup* _group;
	std::string _initial_name;

	Gtk::Entry _name;
	Gtk::CheckButton _active;
	Gtk::CheckButton _gain;
	Gtk::CheckButton _relative;
	Gtk::CheckButton _mute;
	Gtk::CheckButton _solo;
	Gtk::CheckButton _rec_enable;
	Gtk::CheckButton _select;
	Gtk::CheckButton _edit;
	Gtk::CheckButton _route_active;
	Gtk::CheckButton _share_color;
	Gtk::CheckButton _share_monitoring;
	Gtk::Button* _ok;
	Gtk::ColorButton _color;

	void gain_toggled ();
	void update ();
	bool unique_name (std::string const name) const;
};


#endif
