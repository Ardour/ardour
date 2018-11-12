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

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class UIManager;
}

namespace ActionManager {

	LIBGTKMM2EXT_API extern std::string unbound_string;  /* the key string returned if an action is not bound */
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::UIManager> ui_manager;

	LIBGTKMM2EXT_API extern void set_sensitive (std::vector<Glib::RefPtr<Gtk::Action> >& actions, bool);
	LIBGTKMM2EXT_API extern std::string get_key_representation (const std::string& accel_path, Gtk::AccelKey& key);

	LIBGTKMM2EXT_API extern Gtk::Widget* get_widget (const char * name);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> get_action (const char* group, const char* name);
	LIBGTKMM2EXT_API extern Glib::RefPtr<Gtk::Action> get_action (const char* path);

	LIBGTKMM2EXT_API extern void do_action (const char* group, const char* name);
	LIBGTKMM2EXT_API extern void set_toggle_action (const char* group, const char* name, bool);

	LIBGTKMM2EXT_API extern void check_toggleaction (const std::string&);
	LIBGTKMM2EXT_API extern void uncheck_toggleaction (const std::string&);
	LIBGTKMM2EXT_API extern void set_toggleaction_state (const std::string&, bool);
	LIBGTKMM2EXT_API extern bool set_toggleaction_state (const char*, const char*, bool);

	LIBGTKMM2EXT_API extern void save_action_states ();
	LIBGTKMM2EXT_API extern void enable_active_actions ();
	LIBGTKMM2EXT_API extern void disable_active_actions ();
};

#endif /* __libgtkmm2ext_actions_h__ */
