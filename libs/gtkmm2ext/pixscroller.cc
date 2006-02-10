/*
    Copyright (C) 2005 Paul Davis
 
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
#include <algorithm>
#include <cmath>

#include <gtkmm.h>

#include <gtkmm2ext/pixscroller.h>

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

PixScroller::PixScroller (Adjustment& a, Pix& pix)
	: adj (a)
{
	dragging = false;
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);

	adj.signal_value_changed().connect (mem_fun (*this, &PixScroller::adjustment_changed));
	default_value = adj.get_value();

	pix.generate ();

	rail = *(pix.pixmap (0));
	rail_mask = *(pix.shape_mask (0));
	slider = *(pix.pixmap (1));
	slider_mask = *(pix.shape_mask (1));

	int w, h;

	slider->get_size (w, h);
	sliderrect.set_width(w);
	sliderrect.set_height(h);
	rail->get_size (w, h);
	railrect.set_width(w);
	railrect.set_height(h);

	railrect.set_y(sliderrect.get_height() / 2);
	sliderrect.set_x(0);

	overall_height = railrect.get_height() + sliderrect.get_height();

	sliderrect.set_y((int) rint ((overall_height - sliderrect.get_height()) * (adj.get_upper() - adj.get_value())));
	railrect.set_x((sliderrect.get_width() / 2) - 2);
}

void
PixScroller::on_size_request (GtkRequisition* requisition)
{
	requisition->width = sliderrect.get_width();
	requisition->height = overall_height;
}

bool
PixScroller::on_expose_event (GdkEventExpose* ev)
{
	GdkRectangle intersect;
	Glib::RefPtr<Gdk::Window> win (get_window());

	win->draw_rectangle (get_style()->get_bg_gc(get_state()), TRUE, 
			    ev->area.x,
			    ev->area.y,
			    ev->area.width,
			    ev->area.height);

	if (gdk_rectangle_intersect (railrect.gobj(), &ev->area, &intersect)) {
		Glib::RefPtr<Gdk::GC> gc(get_style()->get_bg_gc(get_state()));
		win->draw_drawable (gc, rail, 
				 intersect.x - railrect.get_x(),
				 intersect.y - railrect.get_y(),
				 intersect.x, 
				 intersect.y, 
				 intersect.width,
				 intersect.height);
	}
	
	if (gdk_rectangle_intersect (sliderrect.gobj(), &ev->area, &intersect)) {
		Glib::RefPtr<Gdk::GC> gc(get_style()->get_fg_gc(get_state()));
		Glib::RefPtr<Gdk::Bitmap> mask (slider_mask);
// Do these have a gtk2 equivalent?
//		Gdk::GCValues values;
//		gc->get_values(values);
		gc->set_clip_origin (sliderrect.get_x(), sliderrect.get_y());
		gc->set_clip_mask (mask);
		win->draw_drawable (gc, slider, 
				 intersect.x - sliderrect.get_x(),
				 intersect.y - sliderrect.get_y(),
				 intersect.x, 
				 intersect.y, 
				 intersect.width,
				 intersect.height);
//		gc->set_clip_origin(values.clip_x_origin, values.clip_y_origin);
//		Gdk::Bitmap i_hate_gdk (values.clip_mask);
//		gc->set_clip_mask (i_hate_gdk);
	}


	return true;
}

bool
PixScroller::on_button_press_event (GdkEventButton* ev)
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
PixScroller::on_button_release_event (GdkEventButton* ev)
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

				if (ev->y < sliderrect.get_height()/2) {
					/* near the top */
					fract = 1.0;
				} else {
					fract = 1.0 - (ev->y - sliderrect.get_height()/2) / railrect.get_height();
				}

				fract = min (1.0, fract);
				fract = max (0.0, fract);

				adj.set_value (scale * fract * (adj.get_upper() - adj.get_lower()));
			}
		} else {
			if (ev->state & Gdk::SHIFT_MASK) {
				adj.set_value (default_value);
				cerr << "default value = " << default_value << endl;
			}
		}
		break;
	case 4:
		/* wheel up */
		adj.set_value (adj.get_value() + (adj.get_page_increment() * scale));
		break;
	case 5:
		/* wheel down */
		adj.set_value (adj.get_value() - (adj.get_page_increment() * scale));
		break;
	default:
		break;
	}
	return false;
}

bool
PixScroller::on_motion_notify_event (GdkEventMotion* ev)
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

		fract = (delta / railrect.get_height());

		fract = min (1.0, fract);
		fract = max (-1.0, fract);
		
		fract = -fract;

		adj.set_value (adj.get_value() + scale * fract * (adj.get_upper() - adj.get_lower()));
	}

	return true;
}

void
PixScroller::adjustment_changed ()
{
	int y = (int) rint ((overall_height - sliderrect.get_height()) * (adj.get_upper() - adj.get_value()));

	if (y != sliderrect.get_y()) {
		sliderrect.set_y(y);
		queue_draw ();
	}
}
