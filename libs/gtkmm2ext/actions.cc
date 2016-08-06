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
#include <vector>
#include <string>
#include <list>
#include <stack>
#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include <gtk/gtkaccelmap.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkactiongroup.h>

#include <gtkmm.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include <glibmm/miscutils.h>

#include "pbd/error.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace PBD;
using namespace Gtkmm2ext;

RefPtr<UIManager> ActionManager::ui_manager;
string ActionManager::unbound_string = "--";

struct ActionState {
	GtkAction* action;
	bool       sensitive;
	ActionState (GtkAction* a, bool s) : action (a), sensitive (s) {}
};

typedef std::vector<ActionState> ActionStates;

static ActionStates action_states_to_restore;
static bool actions_disabled = false;

void
ActionManager::save_action_states ()
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/
	GList* list = gtk_ui_manager_get_action_groups (ActionManager::ui_manager->gobj());
	GList* node;
	GList* acts;

	for (node = list; node; node = g_list_next (node)) {

		GtkActionGroup* group = (GtkActionGroup*) node->data;

		for (acts = gtk_action_group_list_actions (group); acts; acts = g_list_next (acts)) {
			GtkAction* action = (GtkAction*) acts->data;
			action_states_to_restore.push_back (ActionState (action, gtk_action_get_sensitive (action)));
		}
	}
}

void
ActionManager::enable_active_actions ()
{
	if (!actions_disabled) {
		return ;
	}

	for (ActionStates::iterator i = action_states_to_restore.begin(); i != action_states_to_restore.end(); ++i) {
		if ((*i).action && (*i).sensitive) {
			gtk_action_set_sensitive ((*i).action, true);
		}
	}

	action_states_to_restore.clear ();
	actions_disabled = false;
}

void
ActionManager::disable_active_actions ()
{
	if (actions_disabled == true ) {
		return ;
	}
	// save all action's states to action_states_to_restore
	save_action_states ();

	// set all action's states disabled
	for (ActionStates::iterator i = action_states_to_restore.begin(); i != action_states_to_restore.end(); ++i) {
		if ((*i).sensitive) {
			gtk_action_set_sensitive ((*i).action, false);
		}
	}
	actions_disabled = true;
}

Widget*
ActionManager::get_widget (const char * name)
{
	return ui_manager->get_widget (name);
}

RefPtr<Action>
ActionManager::get_action (const char* path)
{
	if (!path) {
		return RefPtr<Action>();
	}

	/* Skip <Actions>/ in path */

	int len = strlen (path);

	if (len < 3) {
		/* shortest possible path: "a/b" */
		return RefPtr<Action>();
	}

	if (len > 10 && !strncmp (path, "<Actions>/", 10 )) {
		path = path+10;
	} else if (path[0] == '/') {
		path++;
	}

	vector<char> copy(len+1);
	strcpy (&copy[0], path);
	char* slash = strchr (&copy[0], '/');
	if (!slash) {
		return RefPtr<Action> ();
	}
	*slash = '\0';

	return get_action (&copy[0], ++slash);

}

RefPtr<Action>
ActionManager::get_action (const char* group_name, const char* action_name)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	if (! ui_manager) {
		return RefPtr<Action> ();
	}

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

			break;
		}
	}

	return act;
}

void
ActionManager::set_sensitive (vector<RefPtr<Action> >& actions, bool state)
{
	// if actions weren't disabled
	if (!actions_disabled) {
		for (vector<RefPtr<Action> >::iterator i = actions.begin(); i != actions.end(); ++i) {
			(*i)->set_sensitive (state);
		}
	}
	else {
		// actions were disabled
		// so we should just set necessary action's states in action_states_to_restore
		for (vector<RefPtr<Action> >::iterator i = actions.begin(); i != actions.end(); ++i) {
			// go through action_states_to_restore and set state of actions
			for (ActionStates::iterator j = action_states_to_restore.begin(); j != action_states_to_restore.end(); ++j) {
				// all actions should have their individual name, so we can use it for comparison
				if (gtk_action_get_name ((*j).action) == (*i)->get_name ()) {
					(*j).sensitive = state;
				}
			}
		}
	}
}

void
ActionManager::check_toggleaction (string n)
{
	set_toggleaction_state (n, true);
}

void
ActionManager::uncheck_toggleaction (string n)
{
	set_toggleaction_state (n, false);
}

void
ActionManager::set_toggleaction_state (string n, bool s)
{
	char const * name = n.c_str ();

	const char *last_slash = strrchr (name, '/');

	if (last_slash == 0) {
		fatal << string_compose ("programmer error: %1 %2", "illegal toggle action name", name) << endmsg;
		abort(); /*NOTREACHED*/
		return;
	}

	/* 10 = strlen ("<Actions>/") */
	size_t len = last_slash - (name + 10);

	char* group_name = new char[len+1];
	memcpy (group_name, name + 10, len);
	group_name[len] = '\0';

	const char* action_name = last_slash + 1;

	RefPtr<Action> act = get_action (group_name, action_name);
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
		tact->set_active (s);
	} else {
		error << string_compose (_("Unknown action name: %1"),  name) << endmsg;
	}

	delete [] group_name;
}

void
ActionManager::do_action (const char* group, const char*action)
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (group, action);
	if (act) {
		act->activate ();
	}
}

void
ActionManager::set_toggle_action (const char* group, const char*action, bool yn)
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
		if (tact) {
			tact->set_active (yn);
		}
	}
}
