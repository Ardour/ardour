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

#include <ytkmm/box.h>
#include <ytkmm/label.h>
#include <ytkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class Session;
}

class TimeInfoBox;
class RegionEditor;
class RegionFxPropertiesBox;
class RoutePropertiesBox;
class SlotPropertiesBox;

class SelectionPropertiesBox : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	enum DispositionMask {
		ShowRoutes   = 0x01,
		ShowRegions  = 0x02,
		ShowTriggers = 0x04,
		ShowTimeInfo = 0x08,
	};

	SelectionPropertiesBox (DispositionMask dm = DispositionMask(0x07));
	~SelectionPropertiesBox ();

	void set_session (ARDOUR::Session*);

	void add_region_rhs (Gtk::Widget&);
	void remove_region_rhs ();

private:
	void init ();
	void selection_changed ();
	void track_mouse_mode ();
	void delete_region_editor ();

	void on_map ();
	void on_unmap ();

	TimeInfoBox*           _time_info_box;
	RoutePropertiesBox*    _route_prop_box;
	SlotPropertiesBox*     _slot_prop_box;
	Gtk::HBox              _region_editor_box;
	Gtk::Widget*           _region_editor_box_rhs;
	RegionEditor*          _region_editor;
	RegionFxPropertiesBox* _region_fx_box;
	DispositionMask _disposition;

	PBD::ScopedConnection _region_connection;
	PBD::ScopedConnection _editor_connection;
};

