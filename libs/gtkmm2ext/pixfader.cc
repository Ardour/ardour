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
#include "gtkmm2ext/pixfader.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

PixFader::PixFader (Glib::RefPtr<Gdk::Pixbuf> belt, Gtk::Adjustment& adj, int orientation, int fader_length)

	: adjustment (adj),
	  pixbuf (belt),
	  _orien(orientation)
{
        Cairo::Format format;

	dragging = false;
	default_value = adjustment.get_value();
	last_drawn = -1;

	view.x = 0;
	view.y = 0;

	if (orientation == VERT) {
		view.width = girth = pixbuf->get_width();
	} else {
		view.height = girth = pixbuf->get_height();
	}

	set_fader_length (fader_length);

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	adjustment.signal_value_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));
	adjustment.signal_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));

        if (pixbuf->get_has_alpha()) {
                format = Cairo::FORMAT_ARGB32;
        } else {
                format = Cairo::FORMAT_RGB24;
        }
        belt_surface = Cairo::ImageSurface::create  (format, pixbuf->get_width(), pixbuf->get_height());
        belt_context = Cairo::Context::create (belt_surface);
        Gdk::Cairo::set_source_pixbuf (belt_context, pixbuf, 0.0, 0.0);
        belt_context->paint();        

        left_r = 0;
        left_g = 0;
        left_b = 0;

        right_r = 0;
        right_g = 0;
        right_b = 0;
}

PixFader::~PixFader ()
{
}

void
PixFader::set_border_colors (uint32_t left, uint32_t right)
{
        int r, g, b;
        UINT_TO_RGB(left, &r, &g, &b);
        left_r = r/255.0;
        left_g = g/255.0;
        left_b = b/255.0;
        UINT_TO_RGB(right, &r, &g, &b);
        right_r = r/255.0;
        right_g = g/255.0;
        right_b = b/255.0;
}

bool
PixFader::on_expose_event (GdkEventExpose* ev)
{
        Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
	int srcx, srcy;
	int const ds = display_span ();
	int offset_into_pixbuf = (int) floor (span / ((float) span / ds));

	/* account for fader lengths that are shorter than the fader pixbuf */
	if (_orien == VERT) {
		offset_into_pixbuf += pixbuf->get_height() / 2 - view.height;
	} else {
		offset_into_pixbuf += pixbuf->get_width() / 2 - view.width;
	}

        context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
        context->clip ();
	
        if (_orien == VERT) {
                srcx = 0;
                srcy = offset_into_pixbuf;
        } else {
                srcx = offset_into_pixbuf;
                srcy = 0;
        }

        /* fader */

        context->save();
        context->set_source (belt_surface, -srcx, -srcy);
        context->rectangle (0, 0, get_width(), get_height());
        context->clip ();
        context->paint();
        context->restore();

        /* bounding box lines (2 colors for nicer visuals) */
        
        /* top and left side */

        context->set_line_width (1);
        context->set_source_rgb (left_r, left_g, left_b);
        context->move_to (view.width - 1, 0); /* upper right */
        context->line_to (0, 0);              /* upper left */
        context->line_to (0, view.height - 1);/* lower left */
        context->stroke ();

        /* bottom & right side */

        context->set_line_width (1);
        context->set_source_rgb (right_r, right_g, right_b);
        context->move_to (0, view.height - 0.5); /* lower left */
        context->line_to (view.width - 0.5, view.height - 0.5); /* lower right */
        context->line_to (view.width - 0.5, 0); /* upper right */
        context->stroke ();

	/* always draw the unity-position line */


	if (_orien == VERT) {
                context->set_line_width (1); 
                context->set_source_rgb (0.0, 1.0, 0.0);
                context->move_to (1, unity_loc);
                context->line_to (girth, unity_loc);
                context->stroke ();
	} else {
                context->set_line_width (1); 
                context->set_source_rgb (0.0, 1.0, 0.0);
                context->move_to (unity_loc, 1);
                context->line_to (unity_loc, girth - 1);
                context->stroke ();
	}

	last_drawn = ds;

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
	if (ev->type != GDK_BUTTON_PRESS) {
		return true;
	}

	if (ev->button != 1 && ev->button != 2) {
		return false;
	}

	add_modal_grab ();
	grab_loc = (_orien == VERT) ? ev->y : ev->x;
	grab_start = (_orien == VERT) ? ev->y : ev->x;
	grab_window = ev->window;
	dragging = true;

	if (ev->button == 2) {
		set_adjustment_from_event (ev);
	}
	
	return true;
}

bool
PixFader::on_button_release_event (GdkEventButton* ev)
{
	double const ev_pos = (_orien == VERT) ? ev->y : ev->x;
	
	switch (ev->button) {
	case 1:
		if (dragging) {
			remove_modal_grab();
			dragging = false;

			if (ev_pos == grab_start) {

				/* no motion - just a click */

				if (ev->state & Keyboard::TertiaryModifier) {
					adjustment.set_value (default_value);
				} else if (ev->state & Keyboard::GainFineScaleModifier) {
					adjustment.set_value (adjustment.get_lower());
				} else if ((_orien == VERT && ev_pos < span - display_span()) || (_orien == HORIZ && ev_pos > span - display_span())) {
					/* above the current display height, remember X Window coords */
					adjustment.set_value (adjustment.get_value() + adjustment.get_step_increment());
				} else {
					adjustment.set_value (adjustment.get_value() - adjustment.get_step_increment());
				}
			}

		} 
		break;
		
	case 2:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			set_adjustment_from_event (ev);
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
	bool ret = false;

	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale = 0.01;
		} else {
			scale = 0.05;
		}
	} else {
		scale = 0.25;
	}

	if (_orien == VERT) {

		/* should left/right scroll affect vertical faders ? */

		switch (ev->direction) {

		case GDK_SCROLL_UP:
			adjustment.set_value (adjustment.get_value() + (adjustment.get_page_increment() * scale));
			ret = true;
			break;
		case GDK_SCROLL_DOWN:
			adjustment.set_value (adjustment.get_value() - (adjustment.get_page_increment() * scale));
			ret = true;
			break;
		default:
			break;
		}
	} else {

		/* up/down scrolls should definitely affect horizontal faders
		   because they are so much easier to use
		*/

		switch (ev->direction) {

		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_UP:
			adjustment.set_value (adjustment.get_value() + (adjustment.get_page_increment() * scale));
			ret = true;
			break;
		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_DOWN:
			adjustment.set_value (adjustment.get_value() - (adjustment.get_page_increment() * scale));
			ret = true;
			break;
		default:
			break;
		}
	}
	return ret;
}

bool
PixFader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (dragging) {
		double scale = 1.0;
		double const ev_pos = (_orien == VERT) ? ev->y : ev->x;
		
		if (ev->window != grab_window) {
			grab_loc = ev_pos;
			grab_window = ev->window;
			return true;
		}
		
		if (ev->state & Keyboard::GainFineScaleModifier) {
			if (ev->state & Keyboard::GainExtraFineScaleModifier) {
				scale = 0.05;
			} else {
				scale = 0.1;
			}
		}

		double const delta = ev_pos - grab_loc;
		grab_loc = ev_pos;

		double fract = (delta / span);

		fract = min (1.0, fract);
		fract = max (-1.0, fract);

		// X Window is top->bottom for 0..Y
		
		if (_orien == VERT) {
			fract = -fract;
		}

		adjustment.set_value (adjustment.get_value() + scale * fract * (adjustment.get_upper() - adjustment.get_lower()));
	}

	return true;
}

void
PixFader::adjustment_changed ()
{
	if (display_span() != last_drawn) {
		queue_draw ();
	}
}

/** @return pixel offset of the current value from the right or bottom of the fader */
int
PixFader::display_span ()
{
	float fract = (adjustment.get_value () - adjustment.get_lower()) / ((adjustment.get_upper() - adjustment.get_lower()));
	return (_orien != VERT) ? (int)floor (span * (1.0 - fract)) : (int)floor (span * fract);
}

void
PixFader::set_fader_length (int l)
{
	if (_orien == VERT) {
		view.height = span = l;
	} else {
		view.width = span = l;
	}

	update_unity_position ();

	queue_draw ();
}

void
PixFader::update_unity_position ()
{
	if (_orien == VERT) {
		unity_loc = (int) rint (view.height * (1 - (default_value / (adjustment.get_upper() - adjustment.get_lower())))) - 1;
	} else {
		unity_loc = (int) rint (default_value * view.width);
	}

	queue_draw ();
}

bool
PixFader::on_enter_notify_event (GdkEventCrossing*)
{
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
PixFader::on_leave_notify_event (GdkEventCrossing*)
{
	Keyboard::magic_widget_drop_focus();
	return false;
}

void
PixFader::set_adjustment_from_event (GdkEventButton* ev)
{
	double fract = (_orien == VERT) ? (1.0 - (ev->y / span)) : (ev->x / span);

	fract = min (1.0, fract);
	fract = max (0.0, fract);

	adjustment.set_value (fract * (adjustment.get_upper () - adjustment.get_lower ()));
}

void
PixFader::set_default_value (float d)
{
	default_value = d;
	update_unity_position ();
}
