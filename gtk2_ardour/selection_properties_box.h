/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2024 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"


namespace ARDOUR {
	class Session;
}

class TimeInfoBox;
class RegionEditor;
class RoutePropertiesBox;

class SelectionPropertiesBox : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	SelectionPropertiesBox ();
	~SelectionPropertiesBox ();

	void set_session (ARDOUR::Session*);

private:
	void init ();
	void selection_changed ();
	void track_mouse_mode ();

	TimeInfoBox*                _time_info_box;
	RoutePropertiesBox*         _route_prop_box;

	PBD::ScopedConnection _editor_connection;
};

