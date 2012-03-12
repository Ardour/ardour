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
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/accelkey.h>

#include <ardour/configuration.h>

namespace Gtk {
	class UIManager;
}

class ActionManager
{
  public:
	ActionManager() {}
	virtual ~ActionManager () {}

	static void init ();

	static std::vector<Glib::RefPtr<Gtk::Action> > session_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > write_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > region_list_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > plugin_selection_sensitive_actions;

	static std::vector<Glib::RefPtr<Gtk::Action> > region_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > track_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > point_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > time_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > line_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > playlist_selection_sensitive_actions;

	static std::vector<Glib::RefPtr<Gtk::Action> > range_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > transport_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > jack_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > jack_opposite_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > edit_point_in_region_sensitive_actions;

	static void map_some_state (const char* group, const char* action, bool (ARDOUR::Configuration::*get)() const);
	static void toggle_config_state (const char* group, const char* action, bool (ARDOUR::Configuration::*set)(bool), bool (ARDOUR::Configuration::*get)(void) const);
	static void toggle_config_state (const char* group, const char* action, sigc::slot<void> theSlot);

	static void set_sensitive (std::vector<Glib::RefPtr<Gtk::Action> >& actions, bool);

	static std::string unbound_string;  /* the key string returned if an action is not bound */
	static Glib::RefPtr<Gtk::UIManager> ui_manager;

	static Gtk::Widget* get_widget (const char * name);
	static Glib::RefPtr<Gtk::Action> get_action (const char* group, const char* name);
	static Glib::RefPtr<Gtk::Action> get_action (const char* path);

	static void add_action_group (Glib::RefPtr<Gtk::ActionGroup>);

	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   const char * name, const char * label);
	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   const char * name, const char * label, sigc::slot<void> sl, 
						   guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   const char * name, const char * label, sigc::slot<void> sl);
	
	static Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group&, 
							 const char * name, const char * label, sigc::slot<void> sl, 
							 guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group&, 
							 const char * name, const char * label, sigc::slot<void> sl);
	
	static Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  const char * name, const char * label, sigc::slot<void> sl, 
							  guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  const char * name, const char * label, sigc::slot<void> sl);

	static bool lookup_entry (const std::string accel_path, Gtk::AccelKey& key);

	static void get_all_actions (std::vector<std::string>& labels, 
				     std::vector<std::string>& paths, 
				     std::vector<std::string>& keys, 
				     std::vector<Gtk::AccelKey>& bindings);

	static void get_all_actions (std::vector<std::string>& groups, 
				     std::vector<std::string>& paths, 
				     std::vector<Gtk::AccelKey>& bindings);

	static void uncheck_toggleaction (const char * actionname);

	static string get_key_representation (const std::string& accel_path, Gtk::AccelKey& key);
};

#endif /* __ardour_gtk_actions_h__ */
