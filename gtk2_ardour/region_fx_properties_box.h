/*
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
	class RegionFxPlugin;
	class Region;
}

class GenericPluginUI;

class RegionFxPropertiesBox : public Gtk::HBox
{
public:
	RegionFxPropertiesBox (std::shared_ptr<ARDOUR::Region>);
	~RegionFxPropertiesBox ();

private:
	void drop_plugin_uis ();
	void redisplay_plugins ();
	void add_fx_to_display (std::weak_ptr<ARDOUR::RegionFxPlugin>);
	void idle_redisplay_plugins ();

	static int _idle_redisplay_processors (gpointer);

	Gtk::ScrolledWindow _scroller;
	Gtk::HBox           _box;

	std::shared_ptr<ARDOUR::Region> _region;
	std::vector <GenericPluginUI*>  _proc_uis;

	int _idle_redisplay_plugins_id;

	PBD::ScopedConnectionList _processor_connections;
	PBD::ScopedConnection     _region_connection;
		sigc::connection screen_update_connection;
};
