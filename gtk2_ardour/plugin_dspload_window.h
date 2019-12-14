/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_plugin_dspload_window_h__
#define __ardour_plugin_dspload_window_h__

#include <boost/weak_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>

#include "widgets/ardour_button.h"
#include "pbd/signals.h"

#include "ardour_window.h"

namespace ARDOUR {
	class Processor;
}

class PluginLoadStatsGui;

class PluginDSPLoadWindow : public ArdourWindow
{
public:
	PluginDSPLoadWindow ();
	~PluginDSPLoadWindow ();

	void set_session (ARDOUR::Session*);

protected:
	void session_going_away();

	void on_show ();
	void on_hide ();

private:
	void refill_processors ();
	void drop_references ();
	void clear_all_stats ();
	void sort_by_stats (bool);
	void add_processor_to_display (boost::weak_ptr<ARDOUR::Processor>, std::string const&);
	void clear_processor_stats (boost::weak_ptr<ARDOUR::Processor>);

	Gtk::ScrolledWindow _scroller;
	Gtk::VBox _box;
	Gtk::HBox _ctrlbox;
	ArdourWidgets::ArdourButton _reset_button;
	ArdourWidgets::ArdourButton _sort_avg_button;
	ArdourWidgets::ArdourButton _sort_max_button;

	PBD::ScopedConnectionList _processor_connections;
	PBD::ScopedConnectionList _route_connections;
};
#endif

