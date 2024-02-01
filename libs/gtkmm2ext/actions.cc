/*
 * Copyright (C) 2009-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <list>
#include <memory>
#include <stack>


#include <gtk/gtkaccelmap.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkactiongroup.h>

#include <gtkmm.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace PBD;
using namespace Gtkmm2ext;

typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > ActionMap;
static ActionMap actions;
typedef std::vector<Glib::RefPtr<Gtk::ActionGroup> > ActionGroups;
static ActionGroups groups;

RefPtr<UIManager> ActionManager::ui_manager;
string ActionManager::unbound_string = X_("--");

struct ActionState {
	GtkAction* action;
	bool       sensitive;
	ActionState (GtkAction* a, bool s) : action (a), sensitive (s) {}
};

typedef std::vector<ActionState> ActionStates;

static ActionStates action_states_to_restore;
static bool actions_disabled = false;


ActionManager::MissingActionException::MissingActionException (std::string const & str)
	: missing_action_name (str)
{
	std::cerr << "MAE: " << str << std::endl;
}

const char *
ActionManager::MissingActionException::what () const throw ()
{
	/* XXX memory leak */
	return strdup (string_compose ("missing action: %1", missing_action_name).c_str());
}

void
ActionManager::init ()
{
	ui_manager = UIManager::create ();
}

void
ActionManager::save_action_states ()
{
	for (ActionGroups::iterator g = groups.begin(); g != groups.end(); ++g) {

		/* the C++ API for functions used here appears to be broken in
		   gtkmm2.6, so we fall back to the C level.
		*/

		GtkActionGroup* group = (*g)->gobj();

		for (GList* acts = gtk_action_group_list_actions (group); acts; acts = g_list_next (acts)) {
			GtkAction* action = (GtkAction*) acts->data;
			action_states_to_restore.push_back (ActionState (action, gtk_action_get_sensitive (action)));
		}
	}
}

void
ActionManager::set_sensitive (Glib::RefPtr<ActionGroup> group, bool yn)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	GtkActionGroup* grp = group->gobj();

	if (grp) {
		for (GList* acts = gtk_action_group_list_actions (grp); acts; acts = g_list_next (acts)) {
			GtkAction* action = (GtkAction*) acts->data;
			gtk_action_set_sensitive (action, yn);
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
ActionManager::check_toggleaction (const string& n)
{
	set_toggleaction_state (n, true);
}

void
ActionManager::uncheck_toggleaction (const string& n)
{
	set_toggleaction_state (n, false);
}

void
ActionManager::set_toggleaction_state (const string& n, bool s)
{
	string::size_type pos = n.find ('/');

	if (pos == string::npos || pos == n.size() - 1) {
		error << string_compose ("illegal action name \"%1\" passed to ActionManager::set_toggleaction_state()", n) << endmsg;
		return;
	}

	if (!set_toggleaction_state (n.substr (0, pos).c_str(), n.substr (pos+1).c_str(), s)) {
		error << string_compose (_("Unknown action name: %1/%2"), n.substr (0, pos), n.substr (pos+1)) << endmsg;
	}
}

bool
ActionManager::set_toggleaction_state (const char* group_name, const char* action_name, bool s)
{
	RefPtr<Action> act = get_action (group_name, action_name);
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact) {
			tact->set_active (s);
			return true;
		}
	}
	return false;
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
	Glib::RefPtr<Gtk::ToggleAction> tact = ActionManager::get_toggle_action (group, action);
	tact->set_active (yn);
}

RefPtr<Action>
ActionManager::get_action (const string& name, bool or_die)
{
	ActionMap::const_iterator a = actions.find (name);

	if (a != actions.end()) {
		return a->second;
	}

	if (or_die) {
		throw MissingActionException (name);
	}

	cerr << "Failed to find action: [" << name << ']' << endl;
	return RefPtr<Action>();
}

RefPtr<ToggleAction>
ActionManager::get_toggle_action (const string& name, bool or_die)
{
	RefPtr<Action> act = get_action (name, or_die);

	if (!act) {
		return RefPtr<ToggleAction>();
	}

	return Glib::RefPtr<ToggleAction>::cast_dynamic (act);
}

RefPtr<RadioAction>
ActionManager::get_radio_action (const string& name, bool or_die)
{
	RefPtr<Action> act = get_action (name, or_die);

	if (!act) {
		return RefPtr<RadioAction>();
	}

	return Glib::RefPtr<RadioAction>::cast_dynamic (act);
}

RefPtr<Action>
ActionManager::get_action (char const * group_name, char const * action_name, bool or_die)
{
	string fullpath (group_name);
	fullpath += '/';
	fullpath += action_name;

	ActionMap::const_iterator a = actions.find (fullpath);

	if (a != actions.end()) {
		return a->second;
	}

	if (or_die) {
		throw MissingActionException (string_compose ("%1/%2", group_name, action_name));
	}

	cerr << "Failed to find action (2): [" << fullpath << ']' << endl;
	PBD::stacktrace (std::cerr, 20);
	return RefPtr<Action>();
}

RefPtr<ToggleAction>
ActionManager::get_toggle_action (char const * group_name, char const * action_name, bool or_die)
{
	RefPtr<ToggleAction> act = Glib::RefPtr<ToggleAction>::cast_dynamic (get_action (group_name, action_name, or_die));

	if (act) {
		return act;
	}

	if (or_die) {
		throw MissingActionException (string_compose ("%1/%2", group_name, action_name));
	}

	return RefPtr<ToggleAction>();
}

RefPtr<RadioAction>
ActionManager::get_radio_action (char const * group_name, char const * action_name, bool or_die)
{
	RefPtr<RadioAction> act = Glib::RefPtr<RadioAction>::cast_dynamic (get_action (group_name, action_name, or_die));

	if (act) {
		return act;
	}

	if (or_die) {
		throw MissingActionException (string_compose ("%1/%2", group_name, action_name));
	}

	return RefPtr<RadioAction>();
}

RefPtr<ActionGroup>
ActionManager::create_action_group (void * owner, string const & name)
{
	for (ActionGroups::iterator g = groups.begin(); g != groups.end(); ++g) {
		if ((*g)->get_name () == name) {
			return *g;
		}
	}

	RefPtr<ActionGroup> g = ActionGroup::create (name);

	g->set_data (X_("owner"), owner);
	groups.push_back (g);

	/* this is one of the places where our own Action management code
	   has to touch the GTK one, because we want the GtkUIManager to
	   be able to create widgets (particularly Menus) from our actions.

	   This is a necessary step for that to happen.
	*/

	if (g) {
		ActionManager::ui_manager->insert_action_group (g);
	}

	return g;
}

RefPtr<ActionGroup>
ActionManager::get_action_group (string const & name)
{
	for (ActionGroups::iterator g = groups.begin(); g != groups.end(); ++g) {
		if ((*g)->get_name () == name) {
			return *g;
		}
	}

	return RefPtr<ActionGroup> ();
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, const char* name, const char* label)
{
	string fullpath;

	RefPtr<Action> act = Action::create (name, label);

	fullpath = group->get_name();
	fullpath += '/';
	fullpath += name;

	if (actions.insert (ActionMap::value_type (fullpath, act)).second) {
		group->add (act);
		return act;
	}

	/* already registered */
	return RefPtr<Action> ();
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group,
                                const char* name, const char* label,
                                sigc::slot<void> sl)
{
	string fullpath;

	RefPtr<Action> act = Action::create (name, label);

	fullpath = group->get_name();
	fullpath += '/';
	fullpath += name;

	if (actions.insert (ActionMap::value_type (fullpath, act)).second) {
		group->add (act, sl);
		return act;
	}

	/* already registered */
	return RefPtr<Action>();
}

RefPtr<Action>
ActionManager::register_radio_action (RefPtr<ActionGroup> group,
                                      Gtk::RadioAction::Group& rgroup,
                                      const char* name, const char* label,
                                      sigc::slot<void> sl)
{
	string fullpath;

	RefPtr<Action> act = RadioAction::create (rgroup, name, label);
	RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);

	fullpath = group->get_name();
	fullpath += '/';
	fullpath += name;

	if (actions.insert (ActionMap::value_type (fullpath, act)).second) {
		group->add (act, sl);
		return act;
	}

	/* already registered */
	return RefPtr<Action>();
}

RefPtr<Action>
ActionManager::register_radio_action (RefPtr<ActionGroup> group,
                                      Gtk::RadioAction::Group& rgroup,
                                      const char* name, const char* label,
                                      sigc::slot<void,GtkAction*> sl,
                                      int value)
{
	string fullpath;

	RefPtr<Action> act = RadioAction::create (rgroup, name, label);
	RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
	ract->property_value() = value;

	fullpath = group->get_name();
	fullpath += '/';
	fullpath += name;

	if (actions.insert (ActionMap::value_type (fullpath, act)).second) {
		group->add (act, sigc::bind (sl, act->gobj()));
		return act;
	}

	/* already registered */

	return RefPtr<Action>();
}

RefPtr<Action>
ActionManager::register_toggle_action (RefPtr<ActionGroup> group,
                                   const char* name, const char* label, sigc::slot<void> sl)
{
	string fullpath;

	fullpath = group->get_name();
	fullpath += '/';
	fullpath += name;

	RefPtr<Action> act = ToggleAction::create (name, label);

	if (actions.insert (ActionMap::value_type (fullpath, act)).second) {
		group->add (act, sl);
		return act;
	}

	/* already registered */
	return RefPtr<Action>();
}

void
ActionManager::get_actions (void* owner, std::vector<Glib::RefPtr<Gtk::Action> >& acts)
{
	for (ActionMap::const_iterator a = actions.begin(); a != actions.end(); ++a) {
		if (owner) {
			Glib::RefPtr<Gtk::ActionGroup> group = a->second->property_action_group ();
			if (group->get_data (X_("owner")) == owner) {
				acts.push_back (a->second);
			}
		} else {
			acts.push_back (a->second);
		}
	}
}

void
ActionManager::get_all_actions (std::vector<std::string>& paths,
                            std::vector<std::string>& labels,
                            std::vector<std::string>& tooltips,
                            std::vector<std::string>& keys,
                            std::vector<RefPtr<Action> >& acts)
{
	for (ActionMap::const_iterator a = actions.begin(); a != actions.end(); ++a) {

		Glib::RefPtr<Action> act = a->second;

		/* strip the GTK-added <Actions>/ from the front */
		paths.push_back (act->get_accel_path().substr (10));
		labels.push_back (act->get_label());
		tooltips.push_back (act->get_tooltip());
		acts.push_back (act);

		/* foreach binding */

#if 0
		Bindings* bindings = (*map)->bindings();

		if (bindings) {

			KeyboardKey key;
			Bindings::Operation op;

			key = bindings->get_binding_for_action (*act, op);

			if (key == KeyboardKey::null_key()) {
				keys.push_back (string());
			} else {
				keys.push_back (key.display_label());
			}
		} else {
			keys.push_back (string());
		}
#else
		keys.push_back (string());
#endif
	}
}
