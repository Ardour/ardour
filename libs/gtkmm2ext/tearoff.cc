/*
    Copyright (C) 2003 Paul Barton-Davis
 
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

#include <cmath>
#include <iostream>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/utils.h>

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace std;

TearOff::TearOff (Widget& c, bool allow_resize)
	: contents (c),
	  own_window (Gtk::WINDOW_TOPLEVEL),
	  tearoff_arrow (ARROW_DOWN, SHADOW_OUT),
	  close_arrow (ARROW_UP, SHADOW_OUT)
{
	dragging = false;
	_visible = true;
	_can_be_torn_off = true;

	tearoff_event_box.add (tearoff_arrow);
	tearoff_event_box.set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);
	tearoff_event_box.signal_button_release_event().connect (mem_fun (*this, &TearOff::tearoff_click));

	close_event_box.add (close_arrow);
	close_event_box.set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);
	close_event_box.signal_button_release_event().connect (mem_fun (*this, &TearOff::close_click));
	
	own_window.add_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK|POINTER_MOTION_MASK|POINTER_MOTION_HINT_MASK);
	own_window.set_resizable (allow_resize);
	own_window.set_type_hint (WINDOW_TYPE_HINT_TOOLBAR);
	
	VBox* box1;
	box1 = manage (new VBox);
	box1->pack_start (close_event_box, false, false, 2);
	
	window_box.pack_end (*box1, false, false, 2);
	own_window.add (window_box);
	
	own_window.signal_button_press_event().connect (mem_fun (*this, &TearOff::window_button_press));
	own_window.signal_button_release_event().connect (mem_fun (*this, &TearOff::window_button_release));
	own_window.signal_motion_notify_event().connect (mem_fun (*this, &TearOff::window_motion));
	own_window.signal_delete_event().connect (mem_fun (*this, &TearOff::window_delete_event));
	own_window.signal_realize().connect (bind (sigc::ptr_fun (Gtkmm2ext::set_decoration), &own_window, WMDecoration (DECOR_BORDER|DECOR_RESIZEH)));

	tearoff_arrow.set_name ("TearOffArrow");
	close_arrow.set_name ("TearOffArrow");

	VBox* box2;
	box2 = manage (new VBox);
	box2->pack_start (tearoff_event_box, false, false, 2);

	pack_start (contents);
	pack_start (*box2, false, false, 2);

}

TearOff::~TearOff ()
{
}

void
TearOff::set_can_be_torn_off (bool yn)
{
	if (yn != _can_be_torn_off) {
		if (yn) {
			tearoff_arrow.set_no_show_all (false);
			tearoff_arrow.show ();
		} else {
			tearoff_arrow.set_no_show_all (true);
			tearoff_arrow.hide ();
		}
		_can_be_torn_off = yn;
	}
}

void
TearOff::set_visible (bool yn)
{
	/* don't change visibility if torn off */

	if (own_window.is_visible()) {
		return;
	}

	if (_visible != yn) {
		_visible = yn;
		if (yn) {
			show_all();
			Visible ();
		} else {
			hide ();
			Hidden ();
		}
	}
}

gint
TearOff::tearoff_click (GdkEventButton* /*ev*/)
{
	if (_can_be_torn_off) {
		remove (contents);
		window_box.pack_start (contents);
		own_window.set_name (get_name());
		close_event_box.set_name (get_name());
		own_window.show_all ();
		own_window.present ();
		std::cerr << "own window should be visible\n";
		hide ();
		Detach ();
	}

	return true;
}

gint
TearOff::close_click (GdkEventButton* /*ev*/)
{
	window_box.remove (contents);
	pack_start (contents);
	reorder_child (contents, 0);
	own_window.hide ();
	show_all ();
	Attach ();
	return true;
}		

gint
TearOff::window_button_press (GdkEventButton* ev)
{
	if (dragging || ev->button != 1) {
		dragging = false;
		own_window.remove_modal_grab();
		return true;
	}

	dragging = true;
	drag_x = ev->x_root;
	drag_y = ev->y_root;

	own_window.add_modal_grab();

	return true;
}

gint
TearOff::window_button_release (GdkEventButton* /*ev*/)
{
	dragging = false;
	own_window.remove_modal_grab();
	return true;
}

gint
TearOff::window_delete_event (GdkEventAny* /*ev*/)
{
	return close_click(0);
}

gint
TearOff::window_motion (GdkEventMotion* ev)
{
	gint x;
	gint y;
	gint mx, my;
	double x_delta;
	double y_delta;
	RefPtr<Gdk::Window> win (own_window.get_window());
	
	own_window.get_pointer (mx, my);

	if (!dragging) {
		return true;
	}

	if (!(ev->state & GDK_BUTTON1_MASK)) {
		dragging = false;
		own_window.remove_modal_grab();
		return true;
	}

	x_delta = ev->x_root - drag_x;
	y_delta = ev->y_root - drag_y;

	win->get_root_origin (x, y);
	win->move ((gint) floor (x + x_delta), (gint) floor (y + y_delta));
	
	drag_x = ev->x_root;
	drag_y = ev->y_root;
	
	return true;
}

bool
TearOff::torn_off() const
{
	return own_window.is_visible();
}
