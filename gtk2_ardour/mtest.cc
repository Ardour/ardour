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

void
printit (string txt)
{
	cout << "This is the " << txt << " item\n";
}

Glib::RefPtr<Action>
make_action (Glib::RefPtr<ActionGroup> group, string name, string label, RefPtr<AccelGroup> accels, slot<void> sl, guint key, Gdk::ModifierType mods)
{
	Glib::RefPtr<Action> act;

	act = Action::create (name, label);
	group->add (act, sl);
	AccelMap::add_entry (act->get_accel_path(), key, mods);

	act->set_accel_group (accels);

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
copy_actions (const RefPtr<ActionGroup> src)
{
	RefPtr<ActionGroup> grp = ActionGroup::create (src->get_name());
	
	ListHandle<RefPtr<Action> > group_actions = src->get_actions();
	
	for (ListHandle<RefPtr<Action> >::iterator a = group_actions.begin(); a != group_actions.end(); ++a) {
		RefPtr<Action> act = Action::create ((*a)->get_name(), (*a)->property_label());
		grp->add (act);
	}

	return grp;
}

int
main (int argc, char* argv[])
{
	Main app (argc, argv);
	Window hidden (WINDOW_TOPLEVEL);
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
	shared_uimanager = UIManager::create();

	actions = ActionGroup::create("MyActions");
	other_actions = ActionGroup::create("OtherActions");
	shared_actions = ActionGroup::create("SharedActions");

	uimanager->add_ui_from_file ("mtest.menus");
	other_uimanager->add_ui_from_file ("mtest_other.menus");
	
	// AccelMap::load ("mtest.bindings");

	RefPtr<AccelGroup> accels = hidden.get_accel_group();

	make_action (actions, "TopMenu", "top");
	make_action (actions, "Foo", "foo", accels, bind (sigc::ptr_fun (printit), "foo"), GDK_p, Gdk::ModifierType (0));

	make_action (other_actions, "OTopMenu", "otop");
	make_action (other_actions, "OFoo", "foo", accels, bind (sigc::ptr_fun (printit), "o-foo"), GDK_p, Gdk::ModifierType (0));

	make_action (shared_actions, "Bar", "bar", accels, bind (sigc::ptr_fun (printit), "barshared"), GDK_p, Gdk::CONTROL_MASK);
	RefPtr<Action> act = make_action (shared_actions, "Baz", "baz", accels, bind (sigc::ptr_fun (printit), "baz-shared"), GDK_p, Gdk::SHIFT_MASK);
	
	act->connect_proxy (button);
	act->connect_proxy (other_button);

	uimanager->insert_action_group (copy_actions (actions));
	uimanager->insert_action_group (copy_actions (shared_actions));
	other_uimanager->insert_action_group (copy_actions (other_actions));
	other_uimanager->insert_action_group (copy_actions (shared_actions));

	other_window.add_accel_group (accels);
	window.add_accel_group (accels);

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
