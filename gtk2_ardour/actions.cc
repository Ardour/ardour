#include <vector>
#include <gtk/gtkaccelmap.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "actions.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace ActionManager;

vector<Glib::RefPtr<Gtk::Action> > ActionManager::session_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::region_list_selection_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::region_selection_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::track_selection_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::plugin_selection_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::range_sensitive_actions;
vector<Glib::RefPtr<Gtk::Action> > ActionManager::jack_sensitive_actions;

static vector<Glib::RefPtr<UIManager> > ui_managers;

void
register_ui_manager (Glib::RefPtr<UIManager> uim)
{
	ui_managers.push_back (uim);
}

RefPtr<Action>
register_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_action (group, name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
register_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl)
{
	RefPtr<Action> act = register_action (group, name, label);
	group->add (act, sl);

	return act;
}


RefPtr<Action>
register_radio_action (RefPtr<ActionGroup> group, RadioAction::Group rgroup, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_radio_action (group, rgroup, name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
register_radio_action (RefPtr<ActionGroup> group, RadioAction::Group rgroup, string name, string label, slot<void> sl)
{
	RefPtr<Action> act;

	act = RadioAction::create (rgroup, name, label);
	group->add (act, sl);

	return act;
}


RefPtr<Action>
register_toggle_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	RefPtr<Action> act = register_toggle_action (group,name, label, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	return act;
}

RefPtr<Action>
register_toggle_action (RefPtr<ActionGroup> group, string name, string label, slot<void> sl)
{
	RefPtr<Action> act;

	act = ToggleAction::create (name, label);
	group->add (act, sl);

	return act;
}

RefPtr<Action>
register_action (RefPtr<ActionGroup> group, string name, string label)
{
	RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act);

	return act;
}

bool lookup_entry (const Glib::ustring accel_path, Gtk::AccelKey& key)
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
get_all_actions (vector<string>& names, vector<string>& paths, vector<string>& keys, vector<AccelKey>& bindings)
{
	for (vector<Glib::RefPtr<UIManager> >::iterator u = ui_managers.begin(); u != ui_managers.end(); ++u) {

		ListHandle<RefPtr<ActionGroup> > uim_groups = (*u)->get_action_groups ();

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
					keys.push_back ((*u)->get_accel_group()->name (key.get_key(), Gdk::ModifierType (key.get_mod())));
				} else {
					keys.push_back ("--");
				}

				bindings.push_back (AccelKey (key.get_key(), Gdk::ModifierType (key.get_mod())));
			}
		}
	}
}
