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

#include <vector>
#include <string>
#include <list>

#include <gtk/gtkaccelmap.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkactiongroup.h>

#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include <pbd/error.h>
#include <pbd/file_utils.h>

#include <ardour/ardour.h>
#include <ardour/filesystem_paths.h>

#include "actions.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace PBD;
using namespace ARDOUR;

vector<RefPtr<Gtk::Action> > ActionManager::session_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::region_list_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::plugin_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::region_selection_sensitive_actions;
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

RefPtr<UIManager> ActionManager::ui_manager;
string ActionManager::unbound_string = "--";

void
ActionManager::init ()
{
	ui_manager = UIManager::create ();

	sys::path ui_file;

	SearchPath spath = ardour_search_path() + user_config_directory() + system_config_search_path();

	find_file_in_search_path (spath, "ardour.menus", ui_file);

	bool loaded = false;
	
	try {
		ui_manager->add_ui_from_file (ui_file.to_string());
		loaded = true;
	} catch (Glib::MarkupError& err) {
		error << string_compose (_("badly formatted UI definition file: %1"), err.what()) << endmsg;
		cerr << string_compose (_("badly formatted UI definition file: %1"), err.what()) << endl;
	} catch (...) {
		error << _("Ardour menu definition file not found") << endmsg;
	}

	if (!loaded) {
		error << _("ardour will not work without a valid ardour.menus file") << endmsg;
		exit(1);
	}
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, const char * name, const char * label, slot<void> sl)
{
	RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act, sl);

	return act;
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, const char * name, const char * label)
{
	RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act);

	return act;
}


RefPtr<Action>
ActionManager::register_radio_action (RefPtr<ActionGroup> group, RadioAction::Group& rgroup, const char * name, const char * label, slot<void> sl)
{
	RefPtr<Action> act;

	act = RadioAction::create (rgroup, name, label);
	group->add (act, sl);

	return act;
}

RefPtr<Action>
ActionManager::register_toggle_action (RefPtr<ActionGroup> group, const char * name, const char * label, slot<void> sl)
{
	RefPtr<Action> act;

	act = ToggleAction::create (name, label);
	group->add (act, sl);

	return act;
}

bool 
ActionManager::lookup_entry (const ustring accel_path, Gtk::AccelKey& key)
{
	GtkAccelKey gkey;
	bool known = gtk_accel_map_lookup_entry (accel_path.c_str(), &gkey);
	
	if (known) {
		key = AccelKey (gkey.accel_key, Gdk::ModifierType (gkey.accel_mods));
	} else {
		key = AccelKey (GDK_VoidSymbol, Gdk::ModifierType (0));
	}

	return known;
}

struct SortActionsByLabel {
    bool operator() (Glib::RefPtr<Gtk::Action> a, Glib::RefPtr<Gtk::Action> b) {
	    ustring astr = a->get_accel_path();
	    ustring bstr = b->get_accel_path();
	    return astr < bstr;
    }
};

void
ActionManager::get_all_actions (vector<string>& names, vector<string>& paths, vector<string>& keys, vector<AccelKey>& bindings)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	GList* list = gtk_ui_manager_get_action_groups (ui_manager->gobj());
	GList* node;
	GList* acts;

	for (node = list; node; node = g_list_next (node)) {
		
		GtkActionGroup* group = (GtkActionGroup*) node->data;
		
		/* first pass: collect them all */
		
		typedef std::list<Glib::RefPtr<Gtk::Action> > action_list;
		action_list the_acts;

		for (acts = gtk_action_group_list_actions (group); acts; acts = g_list_next (acts)) {
			GtkAction* action = (GtkAction*) acts->data;
			the_acts.push_back (Glib::wrap (action, true));
		}
		
		/* now sort by label */
		
		SortActionsByLabel cmp;
		the_acts.sort (cmp);

		for (action_list::iterator a = the_acts.begin(); a != the_acts.end(); ++a) {

			string accel_path = (*a)->get_accel_path ();
			ustring label = (*a)->property_label();

			names.push_back (label);
			paths.push_back (accel_path);
			
			AccelKey key;
			bool known = lookup_entry (accel_path, key);
			
			if (known) {
				keys.push_back (ui_manager->get_accel_group()->name (key.get_key(), Gdk::ModifierType (key.get_mod())));
			} else {
				keys.push_back (unbound_string);
			}
			
			bindings.push_back (AccelKey (key.get_key(), Gdk::ModifierType (key.get_mod())));
		}
	}
}

void
ActionManager::add_action_group (RefPtr<ActionGroup> grp)
{
	ui_manager->insert_action_group (grp);
}

Widget*
ActionManager::get_widget (const char * name)
{
	return ui_manager->get_widget (name);
}

RefPtr<Action>
ActionManager::get_action (const char* group_name, const char* action_name)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	GList* list = gtk_ui_manager_get_action_groups (ui_manager->gobj());
	GList* node;
	RefPtr<Action> act;

	for (node = list; node; node = g_list_next (node)) {

		GtkActionGroup* _ag = (GtkActionGroup*) node->data;
		
		if (strcmp (group_name,  gtk_action_group_get_name (_ag)) == 0) {
			
			GtkAction* _act;
			
			if ((_act = gtk_action_group_get_action (_ag, action_name)) != 0) {
				act = Glib::wrap (_act, true);
				break;
			}
		}
	}

	return act;
}

void 
ActionManager::set_sensitive (vector<RefPtr<Action> >& actions, bool state)
{
	for (vector<RefPtr<Action> >::iterator i = actions.begin(); i != actions.end(); ++i) {
		(*i)->set_sensitive (state);
	}
}

void
ActionManager::uncheck_toggleaction (const char * name)
{
	char *last_slash = strrchr (name, '/');

	if (last_slash == 0) {
		fatal << string_compose (_("programmer error: %1 %2"), X_("illegal toggle action name"), name) << endmsg;
		/*NOTREACHED*/
		return;
	}

	/* 10 = strlen ("<Actions>/") */
	size_t len = last_slash - (name + 10);

	char* group_name = new char[len+1];
	memcpy (group_name, name + 10, len);
	group_name[len] = '\0';

	char* action_name = last_slash + 1;

        RefPtr<Action> act = get_action (group_name, action_name);
	if (act) {
	        RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
       		tact->set_active (false);
	} else {
		error << string_compose (_("Unknown action name: %1"),  name) << endmsg;
	}

	delete [] group_name;
}

/** Examine the state of a Configuration setting and a toggle action, and toggle the Configuration
 * setting if its state doesn't match the toggle action.
 * @param group Action group.
 * @param action Action name.
 * @param Method to set the state of the Configuration setting.
 * @param Method to get the state of the Configuration setting.
 */
void
ActionManager::toggle_config_state (const char* group, const char* action, bool (Configuration::*set)(bool), bool (Configuration::*get)(void) const)
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
ActionManager::toggle_config_state (const char* group, const char* action, sigc::slot<void> theSlot)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			theSlot ();
		}
	}
}


/** Set the state of a ToggleAction using a particular Configuration get() method
 * @param group Action group.
 * @param action Action name.
 * @param get Method to obtain the state that the ToggleAction should have.
 */
void
ActionManager::map_some_state (const char* group, const char* action, bool (Configuration::*get)() const)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {
			
			bool x = (Config->*get)();

			if (tact->get_active() != x) {
				tact->set_active (x);
			}
		} else {
			cerr << group << ':' << action << " is not a toggle\n";
		}
	} else {
		cerr << group << ':' << action << " not an action\n";
	}
}
