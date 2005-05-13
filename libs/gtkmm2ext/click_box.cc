/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#include <iostream>
#include <cstdio> /* for sprintf, sigh ... */

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

ClickBox::ClickBox (Gtk::Adjustment *adjp, const string &name)
	: AutoSpin (*adjp)
{
	print_func = default_printer;
	print_arg = 0;

	set_name (name);
	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::BUTTON_PRESS_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK);
	set_label ();

	get_adjustment().signal_value_changed().connect (mem_fun (*this, &ClickBox::set_label));

	signal_button_press_event().connect (mem_fun (*this, &ClickBox::button_press_handler));
	signal_button_release_event().connect (mem_fun (*this, &ClickBox::button_release_handler));
}

ClickBox::~ClickBox ()
{
}

bool
ClickBox::button_press_handler (GdkEventButton* ev)
{
	add_modal_grab();
	AutoSpin::button_press (ev);
	return true;
}

bool
ClickBox::button_release_handler (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
	case 2:
	case 3:
		stop_spinning (0);
		remove_modal_grab();
		break;
	default:
		break;
	}
	return true;
}

void
ClickBox::default_printer (char buf[32], Gtk::Adjustment &adj, 
			       void *ignored)
{
	sprintf (buf, "%.2f", adj.get_value());
}

void
ClickBox::set_label ()
{
	queue_draw ();
}

bool
ClickBox::on_expose_event (GdkEventExpose *ev)
{
	/* Why do we do things like this rather than use a Gtk::Label?
	   Because whenever Gtk::Label::set_label() is called, it
	   triggers a recomputation of its own size, along with that
	   of its container and on up the tree. That's intended
	   to be unnecessary here.
	*/

	Gtk::DrawingArea::on_expose_event (ev);

	if (print_func) {

		char buf[32];

		Glib::RefPtr<Gtk::Style> style (get_style());

		print_func (buf, get_adjustment(), print_arg);

		Glib::RefPtr <Gdk::Window> win (get_window());
		win->draw_rectangle (style->get_bg_gc(get_state()),
				     TRUE, 0, 0, -1, -1);

		{
			int width = 0;
			int height = 0;
			create_pango_layout(buf)->get_pixel_size(width, height);
			set_size_request(width, height);
		}
	}

	return true;
}
