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

PixFader::PixFader (Glib::RefPtr<Pixbuf> base, Glib::RefPtr<Pixbuf> handle, Gtk::Adjustment& adj)
	: adjustment (adj),
	  base_pixbuf (base),
	  handle_pixbuf (handle)
{
	dragging = false;
	default_value = adjustment.get_value();
	last_drawn = -1;
	pixrect.x = 0;
	pixrect.y = 0;
	pixrect.width  = base_pixbuf->get_width();
	pixrect.height  = base_pixbuf->get_height();
	pixheight = pixrect.height;

	unity_y = (int) rint (pixrect.height - (default_value * pixrect.height));

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
	GdkRectangle background;

	pixrect.height = display_height ();

	background.x = 0;
	background.y = 0;
	background.width = pixrect.width;
	background.height = pixheight - pixrect.height;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		get_window()->draw_pixbuf (get_style()->get_fg_gc (get_state()), base_pixbuf,
					   intersection.x, intersection.y,
					   0, 0,
					   intersection.width, intersection.height,
					   Gdk::RGB_DITHER_NONE, 0, 0);
					  
	} 

	/* recompute the height of the handle area to use X Window's top->bottom coordinate
	   system.
	*/

	pixrect.y = pixheight - pixrect.height;
	
	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		get_window()->draw_pixbuf(get_style()->get_fg_gc(get_state()), handle_pixbuf, 
					  intersection.x, intersection.y,
					  0, pixrect.y, 
					  intersection.width, intersection.height,
					  Gdk::RGB_DITHER_NONE, 0, 0);
	}

	/* always draw the line */

	get_window()->draw_line (get_style()->get_fg_gc(get_state()), 0, unity_y, pixrect.width, unity_y);

	last_drawn = pixrect.height;
	return true;
}

void
PixFader::on_size_request (GtkRequisition* req)
{
	req->width = base_pixbuf->get_width();
	req->height = base_pixbuf->get_height ();
}

bool
PixFader::on_button_press_event (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
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
				double fract;
				
				if (ev->y < (pixheight/2)) {
					/* near the top */
					fract = 1.0;
				} else {
					fract = 1.0 - (ev->y - pixheight);
				}

				fract = min (1.0, fract);
				fract = max (0.0, fract);

				adjustment.set_value (scale * fract * (adjustment.get_upper() - adjustment.get_lower()));
			}
		} else {
			if (ev->state & Gdk::SHIFT_MASK) {
				adjustment.set_value (default_value);
			}
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

		fract = (delta / pixheight);

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
