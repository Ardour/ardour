#include <vector>
#include <iostream>
#include <gtkmm.h>
#include <gtkmm/accelmap.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkaccelmap.h>

using namespace Gtk;
using namespace std;
using namespace sigc;
using namespace Glib;

struct ActionBinding {
    Glib::ustring             name;
    Glib::ustring             label;
    Gtk::Action::SlotActivate binding;
    guint                     key;
    Gdk::ModifierType         mods;

    ActionBinding (Glib::ustring n, Glib::ustring l, Gtk::Action::SlotActivate b, 
		   guint k = GDK_VoidSymbol, Gdk::ModifierType m = Gdk::ModifierType (0)) 
	    : name (n),
	      label (l),
	      binding (b),
	      key (k),
	      mods (m) {}
};


void
printit (string txt)
{
	cout << "This is the " << txt << " item\n";
}

Glib::RefPtr<Action>
make_action (vector<Glib::RefPtr<ActionGroup> >& groups, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	Glib::RefPtr<Action> last;

	for (vector<RefPtr<ActionGroup> >::iterator g = groups.begin(); g != groups.end(); ++g) {
		Glib::RefPtr<Action> act = Action::create (name, label);
		(*g)->add (act, sl);
		AccelMap::add_entry (act->get_accel_path(), key, mods);
		last = act;
	}

	return last;
}

Glib::RefPtr<Action>
make_action (Glib::RefPtr<ActionGroup> group, Glib::RefPtr<AccelGroup> accel_group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	Glib::RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);
	act->set_accel_group (accel_group);

	cerr << "action " << name << " has path " << act->get_accel_path() << endl;
	
	return act;
}

Glib::RefPtr<Action>
make_action (Glib::RefPtr<ActionGroup> group, string name, string label, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	Glib::RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	cerr << "action " << name << " has path " << act->get_accel_path() << endl;
	
	return act;
}

Glib::RefPtr<Action>
make_action (Glib::RefPtr<ActionGroup> group, string name, string label, slot<void> sl)
{
	Glib::RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act, sl);

	cerr << "action " << name << " has path " << act->get_accel_path() << endl;

	return act;
}

Glib::RefPtr<Action>
make_action (Glib::RefPtr<ActionGroup> group, string name, string label)
{
	Glib::RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act);

	cerr << "action " << name << " has path " << act->get_accel_path() << endl;

	return act;
}

bool 
lookup_entry (const ustring accel_path, Gtk::AccelKey& key)
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

RefPtr<ActionGroup>
make_shared_action_group (ustring name, vector<ActionBinding*>& actions)
{
	RefPtr<ActionGroup> grp = ActionGroup::create (name);

	for (vector<ActionBinding*>::iterator i = actions.begin(); i != actions.end(); ++i) {
		RefPtr<Action> act = Action::create ((*i)->name, (*i)->label);
		grp->add (act);

		if ((*i)->key != GDK_VoidSymbol) {
			Gtk::AccelKey key;

			/* since this is a shared action, only add it once */

			if (!lookup_entry (act->get_accel_path(), key)) {
				AccelMap::add_entry (act->get_accel_path(), (*i)->key, (*i)->mods);
				cerr << "added accel map entry for " << act->get_accel_path() << endl;
			}
		}
	}

	return grp;
}


int
main (int argc, char* argv[])
{
	Main app (argc, argv);
	Window window (WINDOW_TOPLEVEL);
	Window other_window (WINDOW_TOPLEVEL);
	Button button ("click me for baz");
	Button other_button ("click me for baz");
	VBox   vpacker;
	VBox   other_vpacker;

	Glib::RefPtr<ActionGroup> actions;
	Glib::RefPtr<ActionGroup> other_actions;
	Glib::RefPtr<ActionGroup> shared_actions;
	Glib::RefPtr<UIManager> uimanager;
	Glib::RefPtr<UIManager> other_uimanager;
	Glib::RefPtr<UIManager> shared_uimanager;

	window.set_name ("Editor");
	window.set_title ("Editor");

	other_window.set_name ("Other");
	other_window.set_title ("Other");

	uimanager = UIManager::create();
	other_uimanager = UIManager::create();

	actions = ActionGroup::create("MyActions");
	other_actions = ActionGroup::create("OtherActions");

	uimanager->add_ui_from_file ("mtest.menus");
	other_uimanager->add_ui_from_file ("mtest_other.menus");
	
	// AccelMap::load ("mtest.bindings");

	vector<RefPtr<ActionGroup> > all_groups;
	all_groups.push_back (actions);
	all_groups.push_back (other_actions);
	
	make_action (actions, "TopMenu", "top");
	make_action (actions, "Foo", "foo", bind (sigc::ptr_fun (printit), "foo"), GDK_p, Gdk::ModifierType (0));

	make_action (other_actions, "OTopMenu", "otop");
	make_action (other_actions, "OFoo", "foo", bind (sigc::ptr_fun (printit), "o-foo"), GDK_p, Gdk::ModifierType (0));

	vector<ActionBinding*> shared_actions;

	shared_actions.push_back (new ActionBinding ("Bar", "bar", bind (sigc::ptr_fun (printit), "barshared"), GDK_p, Gdk::CONTROL_MASK));
	shared_actions.push_back (new ActionBinding ("Baz", "baz", bind (sigc::ptr_fun (printit), "baz-shared"), GDK_p, Gdk::SHIFT_MASK));

	RefPtr<Action> act = Action::create (shared_actions.back()->name, shared_actions.back()->label);
	
	act->connect_proxy (button);
	act->connect_proxy (other_button);

	uimanager->insert_action_group (actions);
	uimanager->insert_action_group (make_shared_action_group ("shared", shared_actions));
	other_uimanager->insert_action_group (other_actions);
	other_uimanager->insert_action_group (make_shared_action_group ("othershared", shared_actions));

	other_window.add_accel_group (other_uimanager->get_accel_group());
	window.add_accel_group (uimanager->get_accel_group());

	Gtk::MenuBar* m;

	m = dynamic_cast<MenuBar*>(other_uimanager->get_widget ("/OTop"));

	other_vpacker.pack_start (*m);
	other_vpacker.pack_start (other_button);

	other_window.add (other_vpacker);
	other_window.show_all ();

	m = dynamic_cast<MenuBar*>(uimanager->get_widget ("/Top"));

	vpacker.pack_start (*m);
	vpacker.pack_start (button);

	window.add (vpacker);
	window.show_all ();

	Settings::get_default()->property_gtk_can_change_accels() = true;

	AccelMap::save ("mtest.bindings");

	app.run ();

	return 0;
}
