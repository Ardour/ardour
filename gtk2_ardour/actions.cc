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

    $Id$
*/

#include <vector>
#include <gtk/gtkaccelmap.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "actions.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;

vector<RefPtr<Gtk::Action> > ActionManager::session_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::region_list_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::region_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::track_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::plugin_selection_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::range_sensitive_actions;
vector<RefPtr<Gtk::Action> > ActionManager::jack_sensitive_actions;
string ActionManager::unbound_string = "--";


RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_action (group, name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl)
{
	RefPtr<Action> act = register_action (group, name, label);
	group->add (act, sl);

	return act;
}

RefPtr<Action>
ActionManager::register_action (RefPtr<ActionGroup> group, string name, string label)
{
	RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act);

	return act;
}


RefPtr<Action>
ActionManager::register_radio_action (RefPtr<ActionGroup> group, RadioAction::Group rgroup, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_radio_action (group, rgroup, name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
ActionManager::register_radio_action (RefPtr<ActionGroup> group, RadioAction::Group rgroup, string name, string label, slot<void> sl)
{
	RefPtr<Action> act;

	act = RadioAction::create (rgroup, name, label);
	group->add (act, sl);

	return act;
}


RefPtr<Action>
ActionManager::register_toggle_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_toggle_action (group,name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
ActionManager::register_toggle_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl)
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

void
ActionManager::get_all_actions (vector<string>& names, vector<string>& paths, vector<string>& keys, vector<AccelKey>& bindings)
{
	ListHandle<RefPtr<ActionGroup> > uim_groups = ui_manager.get_action_groups ();
	
	for (ListHandle<RefPtr<ActionGroup> >::iterator g = uim_groups.begin(); g != uim_groups.end(); ++g) {
		
		ListHandle<RefPtr<Action> > group_actions = (*g)->get_actions();
		
		for (ListHandle<RefPtr<Action> >::iterator a = group_actions.begin(); a != group_actions.end(); ++a) {
			
			ustring accel_path;
			
			accel_path = (*a)->get_accel_path();
			
			names.push_back ((*a)->get_name());
			paths.push_back (accel_path);
			
			AccelKey key;
			bool known = lookup_entry (accel_path, key);
			
			if (known) {
				keys.push_back (ui_manager.get_accel_group()->name (key.get_key(), Gdk::ModifierType (key.get_mod())));
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
	ui_manager.insert_action_group (grp);
}

Widget*
ActionManager::get_widget (ustring name)
{
	return ui_manager.get_widget (name);
}

RefPtr<Action>
ActionManager::get_action (ustring name)
{
	return ui_manager.get_action (name);
}
