/*
    Copyright (C) 2012 Paul Davis 

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

#include <gtkmm.h>
#include <iostream>

using namespace std;

bool
print_event (GdkEvent* event)
{
	cerr << hex;
	cerr << "Event: type = " << event->type << ' ';

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		cerr << "Button press, button = "
		     << event->button.button
		     << " state "
		     << event->button.state
		     << endl;
		break;

	case GDK_BUTTON_RELEASE:
		cerr << "Button release, button = "
		     << event->button.button
		     << " state "
		     << event->button.state
		     << endl;
		break;

	case GDK_SCROLL:
		cerr << "Scroll: direction = "
		     << event->scroll.direction
		     << " state = "
		     << event->scroll.state
		     << endl;
		break;

	case GDK_KEY_PRESS:
		cerr << "Key press, keycode = "
		     << event->key.keyval
		     << " name "
		     << gdk_keyval_name (event->key.keyval)
		     << " state = "
		     << event->key.state
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
		     << gdk_keyval_name (event->key.keyval)
		     << " state = "
		     << event->key.state
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
