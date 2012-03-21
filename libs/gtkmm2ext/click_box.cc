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

ClickBox::ClickBox (Gtk::Adjustment *adjp, const string &name, bool round_to_steps)
	: AutoSpin (*adjp,0,round_to_steps)
{
	layout = create_pango_layout ("");
	twidth = 0;
	theight = 0;


	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::BUTTON_PRESS_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK);

	get_adjustment().signal_value_changed().connect (mem_fun (*this, &ClickBox::set_label));
	signal_style_changed().connect (mem_fun (*this, &ClickBox::style_changed));
	signal_button_press_event().connect (mem_fun (*this, &ClickBox::button_press_handler));
	signal_button_release_event().connect (mem_fun (*this, &ClickBox::button_release_handler));
	set_name (name);
	set_label ();
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
	default:
	        remove_modal_grab();
		break;
	}
	return true;
}

void
ClickBox::set_label ()
{
	char buf[32];

	bool const h = _printer (buf, get_adjustment());
	if (!h) {
		/* the printer didn't handle it, so use a default */
		sprintf (buf, "%.2f", get_adjustment().get_value ());
	}

	layout->set_text (buf);
	layout->get_pixel_size (twidth, theight);

	queue_draw ();
}

void
ClickBox::style_changed (const Glib::RefPtr<Gtk::Style>&)
{
	layout->context_changed (); 
	layout->get_pixel_size (twidth, theight);
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

	Glib::RefPtr<Gtk::Style> style (get_style());
	Glib::RefPtr<Gdk::GC> fg_gc (style->get_fg_gc (Gtk::STATE_NORMAL));
	Glib::RefPtr<Gdk::GC> bg_gc (style->get_bg_gc (Gtk::STATE_NORMAL));
	Glib::RefPtr<Gdk::Window> win (get_window());
	
	GdkRectangle base_rect;
	GdkRectangle draw_rect;
	gint x, y, width, height, depth;
	
	win->get_geometry (x, y, width, height, depth);
	
	base_rect.width = width;
	base_rect.height = height;
	base_rect.x = 0;
	base_rect.y = 0;
	
	gdk_rectangle_intersect (&ev->area, &base_rect, &draw_rect);
	win->draw_rectangle (bg_gc, true, draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);
	
	if (twidth && theight) {
		win->draw_layout (fg_gc, (width - twidth) / 2, (height - theight) / 2, layout);
	}

	return true;
}

void
ClickBox::set_printer (sigc::slot<bool, char *, Gtk::Adjustment &> p)
{
	_printer = p;
	set_label ();
}

