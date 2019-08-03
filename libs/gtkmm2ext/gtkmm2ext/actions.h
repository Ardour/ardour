/*
 * Copyright (C) 2009-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libgtkmm2ext_actions_h__
#define __libgtkmm2ext_actions_h__

#include <vector>
#include <exception>

#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/accelkey.h>

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class UIManager;
}

namespace ActionManager {

/* Why is this a namespace and not a class?
 *
 * 1) We want it to behave like a singleton without an instance() method. This
 * would normally be accomplished by using a set of static methods and member
 * variables.
 *
 * 2) We need to extend the contents of the (class|namespace) in
 * gtk2_ardour. We can't do this with a class without inheritance, which is not
 * what we're looking for because we want a non-instance singleton.
 *
 * Hence, we use namespacing to allow us to write ActionManager::foobar() as
 * well as the extensions in gtk2_ardour/actions.h
 *
 */

	class LIBGTKMM2EXT_API MissingActionException : public std::exception {
	  public:
		MissingActionException (std::string const & str);
		~MissingActionException() throw() {}
		const char *what() const throw();
	  private:
		std::string missing_action_name;
	};

	LIBGTKMM2EXT_API extern void init ();

	LIBGTKMM2EXT_API extern std::string unbound_string;  /* the key string returned if an action is not bound */
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::UIManager> ui_manager;

	LIBGTKMM2EXT_API extern void set_sensitive (Glib::RefPtr<Gtk::ActionGroup> group, bool yn);
	LIBGTKMM2EXT_API extern void set_sensitive (std::vector<Glib::RefPtr<Gtk::Action> >& actions, bool);
	LIBGTKMM2EXT_API extern std::string get_key_representation (const std::string& accel_path, Gtk::AccelKey& key);
	LIBGTKMM2EXT_API extern Gtk::Widget* get_widget (const char * name);
	LIBGTKMM2EXT_API extern void do_action (const char* group, const char* name);
	LIBGTKMM2EXT_API extern void set_toggle_action (const char* group, const char* name, bool);
	LIBGTKMM2EXT_API extern void check_toggleaction (const std::string&);
	LIBGTKMM2EXT_API extern void uncheck_toggleaction (const std::string&);
	LIBGTKMM2EXT_API extern void set_toggleaction_state (const std::string&, bool);
	LIBGTKMM2EXT_API extern bool set_toggleaction_state (const char*, const char*, bool);
	LIBGTKMM2EXT_API extern void save_action_states ();
	LIBGTKMM2EXT_API extern void enable_active_actions ();
	LIBGTKMM2EXT_API extern void disable_active_actions ();

	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::ActionGroup> create_action_group (void * owner, std::string const & group_name);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::ActionGroup> get_action_group (std::string const & group_name);

	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, const char* name, const char* label);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
	                                                                   const char* name, const char* label, sigc::slot<void> sl);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group,
	                                                 Gtk::RadioAction::Group&,
	                                                 const char* name, const char* label,
	                                                 sigc::slot<void,GtkAction*> sl,
	                                                 int value);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group,
	                                                 Gtk::RadioAction::Group&,
	                                                 const char* name, const char* label,
	                                                 sigc::slot<void> sl);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group,
	                                                  const char* name, const char* label, sigc::slot<void> sl);

	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action>       get_action (const std::string& name, bool or_die = true);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action>       get_action (char const * group_name, char const * action_name, bool or_die = true);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::ToggleAction> get_toggle_action (const std::string& name, bool or_die = true);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::ToggleAction> get_toggle_action (char const * group_name, char const * action_name, bool or_die = true);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::RadioAction>  get_radio_action (const std::string& name, bool or_die = true);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::RadioAction>  get_radio_action (char const * group_name, char const * action_name, bool or_die = true);

	LIBGTKMM2EXT_API extern void get_actions (void* owner, std::vector<Glib::RefPtr<Gtk::Action> >&);

	LIBGTKMM2EXT_API extern void get_all_actions (std::vector<std::string>& paths,
	                             std::vector<std::string>& labels,
	                             std::vector<std::string>& tooltips,
	                             std::vector<std::string>& keys,
	                             std::vector<Glib::RefPtr<Gtk::Action> >& actions);

};

#endif /* __libgtkmm2ext_actions_h__ */
