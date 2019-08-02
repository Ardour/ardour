/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_route_group_dialog_h__
#define __gtk_ardour_route_group_dialog_h__

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>

#include "ardour_dialog.h"
#include "stripable_colorpicker.h"

class RouteGroupDialog : public ArdourDialog
{
public:
	RouteGroupDialog (ARDOUR::RouteGroup *, bool);

	ARDOUR::RouteGroup* group() const { return _group; }
	bool name_check () const;

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
	ArdourColorButton _color;

	void gain_toggled ();
	void update ();
	bool unique_name (std::string const name) const;
};


#endif
