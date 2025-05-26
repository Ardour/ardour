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

#include <vector>

#include <ytkmm/box.h>
#include <ytkmm/scrolledwindow.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class Route;
	class Processor;
	class Session;
}

class GenericPluginUI;

class RoutePropertiesBox : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	RoutePropertiesBox ();
	~RoutePropertiesBox ();

	void set_route (std::shared_ptr<ARDOUR::Route>);

private:
	void property_changed (const PBD::PropertyChange& what_changed);
	void session_going_away ();
	void drop_route ();
	void drop_plugin_uis ();
	void refill_processors ();
	void add_processor_to_display (std::weak_ptr<ARDOUR::Processor> w);
	void idle_refill_processors ();

	static int _idle_refill_processors (gpointer);

	Gtk::ScrolledWindow _scroller;
	Gtk::HBox           _box;

	std::shared_ptr<ARDOUR::Route> _route;
	std::vector <GenericPluginUI*> _proc_uis;

	int _idle_refill_processors_id;

	PBD::ScopedConnectionList _processor_connections;
	PBD::ScopedConnectionList _route_connections;
};

