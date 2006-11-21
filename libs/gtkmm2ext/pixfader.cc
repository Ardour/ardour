/*
    Copyright (C) 2006 Paul Davis 

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

    $Id: fastmeter.h 570 2006-06-07 21:21:21Z sampo $
*/


#include <iostream>
#include <gtkmm2ext/pixfader.h>

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Gdk;
using namespace std;

PixFader::PixFader (Glib::RefPtr<Pixbuf> belt, Gtk::Adjustment& adj)
	: adjustment (adj),
	  pixbuf (belt)
{
	dragging = false;
	default_value = adjustment.get_value();
	last_drawn = -1;
	pixheight = pixbuf->get_height();

	view.x = 0;
	view.y = 0;
	view.width = pixbuf->get_width();
	view.height = pixheight / 2;

	unity_y = (int) rint (view.height - (default_value * view.height));

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);

	adjustment.signal_value_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));
	adjustment.signal_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));
}

PixFader::~PixFader ()
{
}

bool
PixFader::on_expose_event (GdkEventExpose* ev)
{
	GdkRectangle intersection;
	int dh = display_height ();
	int offset_into_pixbuf = (int) floor (view.height / ((float) view.height / dh));

	if (gdk_rectangle_intersect (&view, &ev->area, &intersection)) {
		get_window()->draw_pixbuf(get_style()->get_fg_gc(get_state()), pixbuf, 
					  intersection.x, offset_into_pixbuf + intersection.y,
					  intersection.x, intersection.y,
					  intersection.width, intersection.height,
					  Gdk::RGB_DITHER_NONE, 0, 0);
	}

	/* always draw the line */

	get_window()->draw_line (get_style()->get_fg_gc(get_state()), 0, unity_y, view.width - 2, unity_y);

	last_drawn = dh;
	return true;
}

void
PixFader::on_size_request (GtkRequisition* req)
{
	req->width = view.width;
	req->height = view.height;
}

bool
PixFader::on_button_press_event (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
	case 2:
		if (!(ev->state & Gdk::SHIFT_MASK)) {
			add_modal_grab();
			grab_y = ev->y;
			grab_start = ev->y;
			grab_window = ev->window;
			dragging = true;
		}
		break;
	default:
		break;
	} 
			       

	return false;
}

bool
PixFader::on_button_release_event (GdkEventButton* ev)
{
	double scale;
	double fract;
	
	if (ev->state & GDK_CONTROL_MASK) {
		if (ev->state & GDK_MOD1_MASK) {
			scale = 0.05;
		} else {
			scale = 0.1;
		}
	} else {
		scale = 1.0;
	}

	switch (ev->button) {
	case 1:
		if (dragging) {
			remove_modal_grab();
			dragging = false;

			if (ev->y == grab_start) {
				/* no motion - just a click */

				if (ev->y < view.height - display_height()) {
					/* above the current display height, remember X Window coords */
					adjustment.set_value (adjustment.get_value() + adjustment.get_step_increment());
				} else {
					adjustment.set_value (adjustment.get_value() - adjustment.get_step_increment());
				}
			}

		} else {
			
			if (ev->state & Gdk::SHIFT_MASK) {
				adjustment.set_value (default_value);
			}
		}
		break;
		
	case 2:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			
			fract = 1.0 - (ev->y / view.height); // inverted X Window coordinates, grrr
			
			fract = min (1.0, fract);
			fract = max (0.0, fract);
			
			adjustment.set_value (scale * fract * (adjustment.get_upper() - adjustment.get_lower()));
		}
		break;

	default:
		break;
	}

	return false;
}

bool
PixFader::on_scroll_event (GdkEventScroll* ev)
{
	double scale;
	
	if (ev->state & GDK_CONTROL_MASK) {
		if (ev->state & GDK_MOD1_MASK) {
			scale = 0.05;
		} else {
			scale = 0.1;
		}
	} else {
		scale = 0.5;
	}

	switch (ev->direction) {

	case GDK_SCROLL_UP:
		/* wheel up */
		adjustment.set_value (adjustment.get_value() + (adjustment.get_page_increment() * scale));
		break;
	case GDK_SCROLL_DOWN:
		/* wheel down */
		adjustment.set_value (adjustment.get_value() - (adjustment.get_page_increment() * scale));
		break;
	default:
		break;
	}
	return false;
}

bool
PixFader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (dragging) {
		double fract;
		double delta;
		double scale;

		if (ev->window != grab_window) {
			grab_y = ev->y;
			grab_window = ev->window;
			return true;
		}
		
		if (ev->state & GDK_CONTROL_MASK) {
			if (ev->state & GDK_MOD1_MASK) {
				scale = 0.05;
			} else {
				scale = 0.1;
			}
		} else {
			scale = 1.0;
		}

		delta = ev->y - grab_y;
		grab_y = ev->y;

		fract = (delta / view.height);

		fract = min (1.0, fract);
		fract = max (-1.0, fract);

		// X Window is top->bottom for 0..Y
		
		fract = -fract;

		adjustment.set_value (adjustment.get_value() + scale * fract * (adjustment.get_upper() - adjustment.get_lower()));
	}

	return true;
}

void
PixFader::adjustment_changed ()
{
	if (display_height() != last_drawn) {
		queue_draw ();
	}
}

int
PixFader::display_height ()
{
	float fract = (adjustment.get_upper() - adjustment.get_value ()) / ((adjustment.get_upper() - adjustment.get_lower()));
	return (int) floor (view.height * (1.0 - fract));
}
