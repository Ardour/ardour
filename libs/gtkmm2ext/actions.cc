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
#include <stdint.h>

#include <gtk/gtkaccelmap.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkactiongroup.h>

#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "pbd/error.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace PBD;
using namespace Gtkmm2ext;

RefPtr<UIManager> ActionManager::ui_manager;
string ActionManager::unbound_string = "--";


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
ActionManager::register_radio_action (
	RefPtr<ActionGroup> group, RadioAction::Group& rgroup, string const & name, string const & label, string const & tooltip, slot<void> sl
	)
{
	RefPtr<Action> act;

	act = RadioAction::create (rgroup, name, label, tooltip);
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

RefPtr<Action>
ActionManager::register_toggle_action (RefPtr<ActionGroup> group, string const & name, string const & label, string const & tooltip, slot<void> sl)
{
	RefPtr<Action> act;

	act = ToggleAction::create (name, label, tooltip);
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
ActionManager::get_all_actions (vector<string>& groups, vector<string>& names, vector<string>& tooltips, vector<AccelKey>& bindings)
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

			groups.push_back (gtk_action_group_get_name(group));
			names.push_back (accel_path.substr (accel_path.find_last_of ('/') + 1));
			tooltips.push_back ((*a)->get_tooltip ());

			AccelKey key;
			lookup_entry (accel_path, key);
			bindings.push_back (AccelKey (key.get_key(), Gdk::ModifierType (key.get_mod())));
		}
	}
}

void
ActionManager::get_all_actions (vector<string>& names, vector<string>& paths, vector<string>& tooltips, vector<string>& keys, vector<AccelKey>& bindings)
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

			ustring const label = (*a)->property_label ();
			string const accel_path = (*a)->get_accel_path ();

			names.push_back (label);
			paths.push_back (accel_path);
			tooltips.push_back ((*a)->get_tooltip ());

			AccelKey key;
			keys.push_back (get_key_representation (accel_path, key));
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

	char copy[len+1];
	strcpy (copy, path);
	char* slash = strchr (copy, '/');
	if (!slash) {
		return RefPtr<Action> ();
	}
	*slash = '\0';

	return get_action (copy, ++slash);
	
}

RefPtr<Action>
ActionManager::get_action (const char* group_name, const char* action_name)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	if (ui_manager == 0) {
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
		}
	}

	return act;
}

RefPtr<Action>
ActionManager::get_action_from_name (const char* name)
{
	/* the C++ API for functions used here appears to be broken in
	   gtkmm2.6, so we fall back to the C level.
	*/

	GList* list = gtk_ui_manager_get_action_groups (ui_manager->gobj());
	GList* node;
	GList* acts;

	for (node = list; node; node = g_list_next (node)) {

		GtkActionGroup* group = (GtkActionGroup*) node->data;

		for (acts = gtk_action_group_list_actions (group); acts; acts = g_list_next (acts)) {
			GtkAction* action = (GtkAction*) acts->data;
			if (!strcmp (gtk_action_get_name (action), name)) {
				return Glib::wrap (action, true);
			}
		}
	}

	return RefPtr<Action>();
}

void
ActionManager::set_sensitive (vector<RefPtr<Action> >& actions, bool state)
{
	for (vector<RefPtr<Action> >::iterator i = actions.begin(); i != actions.end(); ++i) {
		(*i)->set_sensitive (state);
	}
}

void
ActionManager::uncheck_toggleaction (string n)
{
	char const * name = n.c_str ();
	
	const char *last_slash = strrchr (name, '/');

	if (last_slash == 0) {
		fatal << string_compose ("programmer error: %1 %2", "illegal toggle action name", name) << endmsg;
		/*NOTREACHED*/
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
       		tact->set_active (false);
	} else {
		error << string_compose (_("Unknown action name: %1"),  name) << endmsg;
	}

	delete [] group_name;
}

string
ActionManager::get_key_representation (const string& accel_path, AccelKey& key)
{
	bool known = lookup_entry (accel_path, key);
	
	if (known) {
		uint32_t k = possibly_translate_legal_accelerator_to_real_key (key.get_key());
		key = AccelKey (k, Gdk::ModifierType (key.get_mod()));
		return ui_manager->get_accel_group()->get_label (key.get_key(), Gdk::ModifierType (key.get_mod()));
	} 
	
	return unbound_string;
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

