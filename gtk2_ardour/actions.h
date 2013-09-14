/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_actions_h__
#define __ardour_gtk_actions_h__

#include <vector>

#include "gtkmm2ext/actions.h"
#include "ardour/rc_configuration.h"

namespace ActionManager {

	/* Ardour specific */

	extern void init ();
	extern void load_menus ();

	extern std::vector<Glib::RefPtr<Gtk::Action> > session_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > write_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > region_list_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > plugin_selection_sensitive_actions;

	extern std::vector<Glib::RefPtr<Gtk::Action> > track_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > point_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > time_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > line_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > playlist_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > mouse_edit_point_requires_canvas_actions;

	extern std::vector<Glib::RefPtr<Gtk::Action> > range_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > transport_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > engine_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > engine_opposite_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > edit_point_in_region_sensitive_actions;

	extern void map_some_state (const char* group, const char* action, bool (ARDOUR::RCConfiguration::*get)() const);
	extern void map_some_state (const char* group, const char* action, sigc::slot<bool>);
	extern void toggle_config_state (const char* group, const char* action, bool (ARDOUR::RCConfiguration::*set)(bool), bool (ARDOUR::RCConfiguration::*get)(void) const);
	extern void toggle_config_state_foo (const char* group, const char* action, sigc::slot<bool, bool>, sigc::slot<bool>);
}


#endif /* __ardour_gtk_actions_h__ */
