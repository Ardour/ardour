#include <iostream>
#include <gtkmm.h>
#include <gtkmm/accelmap.h>

using namespace Gtk;
using namespace std;
using namespace sigc;

void
printit (string txt)
{
	cout << "This is the " << txt << " item\n";
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

	Glib::RefPtr<ActionGroup> shared_actions;
	Glib::RefPtr<ActionGroup> actions;
	Glib::RefPtr<ActionGroup> other_actions;
	Glib::RefPtr<UIManager> uimanager;
	Glib::RefPtr<UIManager> other_uimanager;
	Glib::RefPtr<UIManager> shared_uimanager;
	Glib::RefPtr<AccelGroup> shared_accel_group;

	window.set_name ("Editor");
	window.set_title ("Editor");

	other_window.set_name ("Other");
	other_window.set_title ("Other");

	uimanager = UIManager::create();
	other_uimanager = UIManager::create();
	shared_uimanager = UIManager::create();

	actions = ActionGroup::create("MyActions");
	other_actions = ActionGroup::create("OtherActions");
	shared_actions = ActionGroup::create();

	uimanager->add_ui_from_file ("mtest.menus");
	other_uimanager->add_ui_from_file ("mtest_other.menus");
	
	AccelMap::load ("mtest.bindings");

	make_action (shared_actions, "SharedMenuBar", "shared");
	make_action (shared_actions, "SharedMenu", "sharedm");
	Glib::RefPtr<Action> act = make_action (shared_actions, "Baz", "baz", bind (sigc::ptr_fun (printit), "baz"), GDK_p, Gdk::MOD1_MASK);
	
	act->connect_proxy (button);
	act->connect_proxy (other_button);

	make_action (actions, "TopMenu", "top");
	make_action (actions, "Foo", "foo", bind (sigc::ptr_fun (printit), "foo"), GDK_p, Gdk::ModifierType (0));
	make_action (actions, "Bar", "bar", bind (sigc::ptr_fun (printit), "bar"), GDK_p, Gdk::CONTROL_MASK);
	make_action (other_actions, "OTopMenu", "otop");
	make_action (other_actions, "OFoo", "foo", bind (sigc::ptr_fun (printit), "o-foo"), GDK_p, Gdk::ModifierType (0));
	make_action (other_actions, "OBar", "bar", bind (sigc::ptr_fun (printit), "o-bar"), GDK_p, Gdk::CONTROL_MASK);
	
	other_uimanager->insert_action_group (other_actions);
	other_uimanager->insert_action_group (shared_actions);

	uimanager->insert_action_group (actions);
	uimanager->insert_action_group (shared_actions);
	
	shared_uimanager->insert_action_group (shared_actions);

	other_window.add_accel_group (other_uimanager->get_accel_group());
	other_window.add_accel_group (shared_uimanager->get_accel_group());

	window.add_accel_group (uimanager->get_accel_group());
	window.add_accel_group (shared_uimanager->get_accel_group());

	Gtk::MenuBar* m;

	m = dynamic_cast<MenuBar*>(other_uimanager->get_widget ("/OTop"));

	other_vpacker.pack_start (*m);
	other_vpacker.pack_start (other_button);

	other_window.add (other_vpacker);
	other_window.show_all ();

	m = dynamic_cast<MenuBar*>(uimanager->get_widget ("/Top"));

	vpacker.pack_start (*m);
	vpacker.pack_start (button);

	shared_uimanager->add_ui_from_file ("mtest_shared.menu");

	MenuBar* item = dynamic_cast<MenuBar*> (shared_uimanager->get_widget ("/SharedMenuBar"));

	window.add (vpacker);
	window.show_all ();

	Settings::get_default()->property_gtk_can_change_accels() = true;

	cerr << " shared = " << shared_uimanager->get_accel_group()
	     << " first = " << uimanager->get_accel_group()
	     << " second = " << other_uimanager->get_accel_group ()
	     << endl;

	app.run ();

	return 0;
}
