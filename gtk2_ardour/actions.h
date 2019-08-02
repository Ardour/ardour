/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_actions_h__
#define __ardour_gtk_actions_h__

#include <vector>

#include "gtkmm2ext/actions.h"
#include "ardour/rc_configuration.h"

#include "ui_config.h"

namespace ActionManager {

	/* Ardour specific */

	extern void load_menus (const std::string& menus_file_name); /* not path, just name */

	extern std::vector<Glib::RefPtr<Gtk::Action> > session_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > write_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > region_list_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > plugin_selection_sensitive_actions;

	extern std::vector<Glib::RefPtr<Gtk::Action> > track_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > stripable_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > bus_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > route_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > vca_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > point_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > time_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > line_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > playlist_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > mouse_edit_point_requires_canvas_actions;

	extern std::vector<Glib::RefPtr<Gtk::Action> > range_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > transport_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > engine_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > engine_opposite_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > rec_sensitive_actions;

	extern void map_some_state (const char* group, const char* action, bool (ARDOUR::RCConfiguration::*get)() const);
	extern void map_some_state (const char* group, const char* action, bool (UIConfiguration::*get)() const);
	extern void map_some_state (const char* group, const char* action, sigc::slot<bool>);
	extern void toggle_config_state (const char* group, const char* action, bool (UIConfiguration::*set)(bool), bool (UIConfiguration::*get)(void) const);
	extern void toggle_config_state (const char* group, const char* action, bool (ARDOUR::RCConfiguration::*set)(bool), bool (ARDOUR::RCConfiguration::*get)(void) const);
	extern void toggle_config_state_foo (const char* group, const char* action, sigc::slot<bool, bool>, sigc::slot<bool>);
}


#endif /* __ardour_gtk_actions_h__ */
