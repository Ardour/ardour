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

#include "gtkmm2ext/pixscroller.h"
#include "gtkmm2ext/keyboard.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

PixScroller::PixScroller (Adjustment& a, 
			  Glib::RefPtr<Gdk::Pixbuf> s,
			  Glib::RefPtr<Gdk::Pixbuf> r)
	: adj (a),
	  rail (r),
	  slider (s)
{
        Cairo::Format format;

	dragging = false;
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);

	adj.signal_value_changed().connect (mem_fun (*this, &PixScroller::adjustment_changed));
	default_value = adj.get_value();

	sliderrect.set_width(slider->get_width());
	sliderrect.set_height(slider->get_height());
	railrect.set_width(rail->get_width());
	railrect.set_height(rail->get_height());

	railrect.set_y(sliderrect.get_height() / 2);
	sliderrect.set_x(0);

	overall_height = railrect.get_height() + sliderrect.get_height();

	sliderrect.set_y((int) rint ((overall_height - sliderrect.get_height()) * (adj.get_upper() - adj.get_value())));
	railrect.set_x((sliderrect.get_width() / 2) - 2);

        if (rail->get_has_alpha()) {
                format = Cairo::FORMAT_ARGB32;
        } else {
                format = Cairo::FORMAT_RGB24;
        }
        rail_surface = Cairo::ImageSurface::create  (format, rail->get_width(), rail->get_height());
        rail_context = Cairo::Context::create (rail_surface);
        Gdk::Cairo::set_source_pixbuf (rail_context, rail, 0.0, 0.0);
        rail_context->paint();        

        if (slider->get_has_alpha()) {
                format = Cairo::FORMAT_ARGB32;
        } else {
                format = Cairo::FORMAT_RGB24;
        }
        slider_surface = Cairo::ImageSurface::create  (format, slider->get_width(), slider->get_height());
        slider_context = Cairo::Context::create (slider_surface);
        Gdk::Cairo::set_source_pixbuf (slider_context, slider, 0.0, 0.0);
        slider_context->paint();        
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
        Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
        
	if (gdk_rectangle_intersect (railrect.gobj(), &ev->area, &intersect)) {

                context->save();
                context->rectangle (intersect.x, intersect.y, intersect.width, intersect.height);
                context->clip();
                context->set_source (rail_surface, intersect.x - railrect.get_x(), intersect.y - railrect.get_y());
                context->rectangle (intersect.x, intersect.y, intersect.width, intersect.height);
                context->clip();
                context->paint();
                context->restore();
	}
	
	if (gdk_rectangle_intersect (sliderrect.gobj(), &ev->area, &intersect)) {

                context->save();
                context->rectangle (intersect.x, intersect.y, intersect.width, intersect.height);
                context->clip();
                context->set_source (rail_surface, intersect.x - sliderrect.get_x(), intersect.y - sliderrect.get_y());
                context->rectangle (intersect.x, intersect.y, intersect.width, intersect.height);
                context->clip();
                context->paint();
                context->restore();
	}

	return true;
}

bool
PixScroller::on_button_press_event (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
		if (!(ev->state & Keyboard::TertiaryModifier)) {
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
	
	if (ev->state & Keyboard::PrimaryModifier) {
		if (ev->state & Keyboard::SecondaryModifier) {
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
			if (ev->state & Keyboard::TertiaryModifier) {
				adj.set_value (default_value);
				cerr << "default value = " << default_value << endl;
			}
		}
		break;
	default:
		break;
	}
	return false;
}

bool
PixScroller::on_scroll_event (GdkEventScroll* ev)
{
	double scale;
	
	if (ev->state & Keyboard::PrimaryModifier) {
		if (ev->state & Keyboard::SecondaryModifier) {
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
		adj.set_value (adj.get_value() + (adj.get_page_increment() * scale));
		break;
	case GDK_SCROLL_DOWN:
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
		
		if (ev->state & Keyboard::PrimaryModifier) {
			if (ev->state & Keyboard::SecondaryModifier) {
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
