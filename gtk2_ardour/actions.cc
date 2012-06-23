/*
    Copyright (C) 2005 Paul Davis

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

#include <cstring>
#include <string>
#include <list>

#include <gtk/gtkaccelmap.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkactiongroup.h>

#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/rc_configuration.h"

#include "gtkmm2ext/actions.h"

#include "utils.h"
#include "actions.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

vector<RefPtr<Gtk::Action> > ActionManager::session_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::write_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::region_list_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::plugin_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::track_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::point_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::time_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::line_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::playlist_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::mouse_edit_point_requires_canvas_actions;

vector<RefPtr<Gtk::Action> > ActionManager::range_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::jack_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::jack_opposite_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::transport_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::edit_point_in_region_sensitive_actions;


void
ActionManager::init ()
{
	std::string ui_file;

	ui_manager = UIManager::create ();

	find_file_in_search_path (ardour_config_search_path(), "ardour.menus", ui_file);

	bool loaded = false;

	try {
		ui_manager->add_ui_from_file (ui_file);
		info << string_compose (_("Loading menus from %1"), ui_file) << endmsg;
		loaded = true;
	} catch (Glib::MarkupError& err) {
		error << string_compose (_("badly formatted UI definition file: %1"), err.what()) << endmsg;
		cerr << string_compose (_("badly formatted UI definition file: %1"), err.what()) << endl;
	} catch (...) {
		error << string_compose (_("%1 menu definition file not found"), PROGRAM_NAME) << endmsg;
	}

	if (!loaded) {
		cerr << string_compose (_("%1 will not work without a valid ardour.menus file"), PROGRAM_NAME) << endl;
		error << string_compose (_("%1 will not work without a valid ardour.menus file"), PROGRAM_NAME) << endmsg;
		exit(1);
	}
}

/** Examine the state of a Configuration setting and a toggle action, and toggle the Configuration
 * setting if its state doesn't match the toggle action.
 * @param group Action group.
 * @param action Action name.
 * @param Method to set the state of the Configuration setting.
 * @param Method to get the state of the Configuration setting.
 */
void
ActionManager::toggle_config_state (const char* group, const char* action, bool (RCConfiguration::*set)(bool), bool (RCConfiguration::*get)(void) const)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {
			bool x = (Config->*get)();

			if (x != tact->get_active()) {
				(Config->*set) (!x);
			}
		}
	}
}

void
ActionManager::toggle_config_state_foo (const char* group, const char* action, sigc::slot<bool, bool> set, sigc::slot<bool> get)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {
			bool const x = get ();

			if (x != tact->get_active ()) {
				set (!x);
			}
		}
	}
}


/** Set the state of a ToggleAction using a particular Configuration get() method
 * @param group Action group.
 * @param action Action name.
 * @param get Method to obtain the state that the ToggleAction should have.
 */
void
ActionManager::map_some_state (const char* group, const char* action, bool (RCConfiguration::*get)() const)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {

			bool x = (Config->*get)();

			if (tact->get_active() != x) {
				tact->set_active (x);
			}
		}
	}
}

void
ActionManager::map_some_state (const char* group, const char* action, sigc::slot<bool> get)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {

			bool const x = get ();

			if (tact->get_active() != x) {
				tact->set_active (x);
			}
		}
	}
}

