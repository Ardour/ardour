#include <string>
#include <gtkmm.h>
#include <iostream>

using namespace std;

string
print_state (int state)
{
        string str;

        if (state & GDK_SHIFT_MASK) {
                str += "shift ";
        }
        if (state & GDK_LOCK_MASK) {
                str += "lock ";
        }
        if (state & GDK_CONTROL_MASK) {
                str += "control ";
        }
        if (state & GDK_MOD1_MASK) {
                str += "mod1 ";
        }
        if (state & GDK_MOD2_MASK) {
                str += "mod2 ";
        }
        if (state & GDK_MOD3_MASK) {
                str += "mod3 ";
        }
        if (state & GDK_MOD4_MASK) {
                str += "mod4 ";
        }
        if (state & GDK_MOD5_MASK) {
                str += "mod5 ";
        }
        if (state & GDK_BUTTON1_MASK) {
                str += "button1 ";
        }
        if (state & GDK_BUTTON2_MASK) {
                str += "button2 ";
        }
        if (state & GDK_BUTTON3_MASK) {
                str += "button3 ";
        }
        if (state & GDK_BUTTON4_MASK) {
                str += "button4 ";
        }
        if (state & GDK_BUTTON5_MASK) {
                str += "button5 ";
        }
        if (state & GDK_SUPER_MASK) {
                str += "super ";
        }
        if (state & GDK_HYPER_MASK) {
                str += "hyper ";
        }
        if (state & GDK_META_MASK) {
                str += "meta ";
        }
        if (state & GDK_RELEASE_MASK) {
                str += "release ";
        }

        return str;
}

bool
print_event (GdkEvent* event)
{
	const gchar* kstr;

	cerr << hex;
	cerr << "Event: type = " << event->type << ' ';

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		cerr << "2-Button press, button = " 
		     << event->button.button
		     << " state "
		     << print_state (event->button.state)
		     << endl;
		break;

	case GDK_BUTTON_PRESS:
		cerr << "Button press, button = " 
		     << event->button.button
		     << " state "
		     << print_state (event->button.state)
		     << endl;
		break;

	case GDK_BUTTON_RELEASE:
		cerr << "Button release, button = " 
		     << event->button.button
		     << " state "
		     << print_state (event->button.state)
		     << endl;
		break;

	case GDK_SCROLL:
		cerr << "Scroll: direction = "
		     << event->scroll.direction
		     << " state = "
		     << print_state (event->scroll.state)
		     << endl;
		break;

	case GDK_KEY_PRESS:
		cerr << "Key press, keycode = "
		     << event->key.keyval
		     << " name " 
		     << ((kstr = gdk_keyval_name (event->key.keyval)) ? kstr : "UNKNOWN KEY")
		     << " state = "
		     << print_state (event->key.state)
		     << " hw keycode = "
		     << event->key.hardware_keycode
		     << " string = "
		     << (event->key.string ? event->key.string : "not defined")
		     << endl;
		break;

	case GDK_KEY_RELEASE:
		cerr << "Key release, keycode = "
		     << event->key.keyval
		     << " name " 
		     << ((kstr = gdk_keyval_name (event->key.keyval)) ? kstr : "UNKNOWN KEY")
		     << " state = "
		     << print_state (event->key.state)
		     << " hw keycode = "
		     << event->key.hardware_keycode
		     << " string = "
		     << (event->key.string ? event->key.string : "not defined")
		     << endl;
		break;

	default:
		cerr << endl;
		break;
	}
	cerr << dec;

	return false;
}

int
main (int argc, char* argv[])
{
	Gtk::Main app (&argc, &argv);
	Gtk::Window window;
	Gtk::EventBox eventbox;
	
	window.add (eventbox);
	window.set_size_request (250, 250);

	eventbox.signal_event().connect (sigc::ptr_fun (print_event));
	eventbox.add_events (Gdk::SCROLL_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	eventbox.set_flags (Gtk::CAN_FOCUS);

	eventbox.show ();
	window.show ();
	app.run();
}
