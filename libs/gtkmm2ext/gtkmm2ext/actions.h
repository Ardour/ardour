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

#ifndef __libgtkmm2ext_actions_h__
#define __libgtkmm2ext_actions_h__

#include <vector>

#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/accelkey.h>

namespace Gtk {
	class UIManager;
}

namespace ActionManager {

	extern std::string unbound_string;  /* the key string returned if an action is not bound */
	extern Glib::RefPtr<Gtk::UIManager> ui_manager;
	
	extern void set_sensitive (std::vector<Glib::RefPtr<Gtk::Action> >& actions, bool);
	extern std::string get_key_representation (const std::string& accel_path, Gtk::AccelKey& key);

	extern Gtk::Widget* get_widget (const char * name);
	extern Glib::RefPtr<Gtk::Action> get_action (const char* group, const char* name);
	extern Glib::RefPtr<Gtk::Action> get_action (const char* path);
	extern void do_action (const char* group, const char* name);
	extern void set_toggle_action (const char* group, const char* name, bool);

	extern void add_action_group (Glib::RefPtr<Gtk::ActionGroup>);

	extern Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
						   const char * name, const char * label);
	extern Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
						   const char * name, const char * label, sigc::slot<void> sl,
						   guint key, Gdk::ModifierType mods);
	extern Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
						   const char * name, const char * label, sigc::slot<void> sl);

	extern Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group&,
								const char * name, const char * label, sigc::slot<void> sl,
								guint key, Gdk::ModifierType mods);
	extern Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group&,
								const char * name, const char * label, sigc::slot<void> sl);
	extern Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group&,
								std::string const &, std::string const &, std::string const &, sigc::slot<void> sl);

	extern Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group,
							  const char * name, const char * label, sigc::slot<void> sl,
							  guint key, Gdk::ModifierType mods);
	extern Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group,
								 std::string const &, std::string const &, std::string const &, sigc::slot<void> sl);
	extern Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group,
								 const char * name, const char * label, sigc::slot<void> sl);

	extern bool lookup_entry (const Glib::ustring accel_path, Gtk::AccelKey& key);

	extern void get_all_actions (std::vector<std::string>& names,
				     std::vector<std::string>& paths,
				     std::vector<std::string>& tooltips,
				     std::vector<std::string>& keys,
				     std::vector<Gtk::AccelKey>& bindings);

	extern void get_all_actions (std::vector<std::string>& groups,
				     std::vector<std::string>& paths,
				     std::vector<std::string>& tooltips,
				     std::vector<Gtk::AccelKey>& bindings);

	extern void uncheck_toggleaction (std::string);
};

#endif /* __libgtkmm2ext_actions_h__ */
