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

#include "pbd/stacktrace.h"

#include "gtkmm2ext/pixfader.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/cairo_widget.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

#define CORNER_RADIUS 4
#define CORNER_SIZE   2
#define CORNER_OFFSET 1
#define FADER_RESERVE 5

PixFader::PixFader (Gtk::Adjustment& adj, int orientation, int fader_length, int fader_girth)
	: adjustment (adj)
	, span (fader_length)
	, girth (fader_girth)
	, _orien (orientation)
	, _hovering (false)
	, last_drawn (-1)
	, dragging (false)
{
	bg_gradient = 0;
	fg_gradient = 0;

	default_value = adjustment.get_value();
	update_unity_position ();

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	adjustment.signal_value_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));
	adjustment.signal_changed().connect (mem_fun (*this, &PixFader::adjustment_changed));

	if (_orien == VERT) {
		DrawingArea::set_size_request(girth, span);
	} else {
		DrawingArea::set_size_request(span, girth);
	}
}

PixFader::~PixFader ()
{
}

bool
PixFader::on_expose_event (GdkEventExpose* ev)
{
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
	cairo_t* cr = context->cobj();

	Gdk::Color fg_col = get_style()->get_fg (get_state());

	float ds = display_span ();
	float w = get_width();
	float h = get_height();

	//fill in the bg rect ... 
	Gdk::Color c = get_style()->get_bg (Gtk::STATE_PRELIGHT);  //why prelight?  Shouldn't we be using the parent's color?
	CairoWidget::set_source_rgb_a (cr, c);
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_fill(cr);

	//"slot"
	cairo_set_source_rgba (cr, 0.17, 0.17, 0.17, 1.0);
	Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, CORNER_RADIUS-0.5);
	cairo_fill(cr);

	//mask off the corners
	Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, CORNER_RADIUS-0.5);
	cairo_clip(cr);
	
	if (_orien == VERT) {

		int travel = h - 1;
		int progress = travel * (1.0-ds);
		int top = 1 + progress;
		int bottom = h;
		
		//background gradient
		if ( !CairoWidget::flat_buttons() ) {
			cairo_pattern_t *bg_gradient = cairo_pattern_create_linear (0.0, 0.0, w, 0);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0, 0, 0, 0, 0.4);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0.2, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 1, 0, 0, 0, 0.0);
			cairo_set_source (cr, bg_gradient);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, CORNER_RADIUS-1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(bg_gradient);
		}
		
		//fg color
		CairoWidget::set_source_rgb_a (cr, fg_col, 1.0);
		Gtkmm2ext::rounded_top_rectangle (cr, 1, top, w-2, bottom, CORNER_RADIUS - 1.5);
		cairo_fill(cr);

		//fg gradient
		if (!CairoWidget::flat_buttons() ) {
			cairo_pattern_t *fg_gradient = cairo_pattern_create_linear (0.0, 0.0, w, 0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0.1, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 1, 0, 0, 0, 0.3);
			cairo_set_source (cr, fg_gradient);
			Gtkmm2ext::rounded_rectangle (cr, 1, top, w-2, bottom, CORNER_RADIUS - 1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(fg_gradient);
		}
	} else {

		int travel = w - 1;
		int progress = travel * ds;
		int left = 1;
		int length = progress;
		
		//background gradient
		if ( !CairoWidget::flat_buttons() ) {
			cairo_pattern_t *bg_gradient = cairo_pattern_create_linear (0.0, 0.0, 0, h);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0, 0, 0, 0, 0.4);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 0.2, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (bg_gradient, 1, 0, 0, 0, 0.0);
			cairo_set_source (cr, bg_gradient);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, w-2, h-2, CORNER_RADIUS-1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(bg_gradient);
		}
		
		//fg color
		CairoWidget::set_source_rgb_a (cr, fg_col, 1.0);
		Gtkmm2ext::rounded_rectangle (cr, left, 1, length, h-2, CORNER_RADIUS - 1.5);
		cairo_fill(cr);

		//fg gradient
		if (!CairoWidget::flat_buttons() ) {
			cairo_pattern_t * fg_gradient = cairo_pattern_create_linear (0.0, 0.0, 0, h);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 0.1, 0, 0, 0, 0.0);
			cairo_pattern_add_color_stop_rgba (fg_gradient, 1, 0, 0, 0, 0.3);
			cairo_set_source (cr, fg_gradient);
		Gtkmm2ext::rounded_rectangle (cr, left, 1, length, h-2, CORNER_RADIUS - 1.5);
			cairo_fill (cr);
			cairo_pattern_destroy(fg_gradient);
		}
	}
		
	cairo_reset_clip(cr);

	//black border
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	Gtkmm2ext::rounded_rectangle (cr, 0.5, 0.5, w-1, h-1, CORNER_RADIUS);
	cairo_stroke(cr);

	/* draw the unity-position line if it's not at either end*/
	if (unity_loc > 0) {
		context->set_line_width (1);
		Gdk::Color c = get_style()->get_fg (Gtk::STATE_ACTIVE);
		CairoWidget::set_source_rgb_a (cr, c, 1.0);
		if ( _orien == VERT) {
			if (unity_loc < h ) {
				context->move_to (2.5, unity_loc + CORNER_OFFSET + .5);
				context->line_to (girth-2.5, unity_loc + CORNER_OFFSET + .5);
				context->stroke ();
			}
		} else {
			if ( unity_loc < w ){
				context->move_to (unity_loc - CORNER_OFFSET + .5, 3.5);
				context->line_to (unity_loc - CORNER_OFFSET + .5, girth-3.5);
				context->stroke ();
			}
		}
	}
	
	//draw text
	if ( !_text.empty() ) {

		//calc text size
		if ( !_text.empty()) {
			_layout->get_pixel_size (_text_width, _text_height);
		} else {
			_text_width = 0;
			_text_height = 0;
		}

		/* center text */
		cairo_new_path (cr);
		cairo_move_to (cr, (get_width() - _text_width)/2.0, get_height()/2.0 - _text_height/2.0);
		Gdk::Color c = get_style()->get_text (get_state());
		CairoWidget::set_source_rgb_a (cr, c, 0.9);
		pango_cairo_show_layout (cr, _layout->gobj());
	} 
	
	if (!get_sensitive()) {
		Gtkmm2ext::rounded_rectangle (cr, CORNER_OFFSET, CORNER_OFFSET, w-CORNER_SIZE, h-CORNER_SIZE, CORNER_RADIUS);
		cairo_set_source_rgba (cr, 0.505, 0.517, 0.525, 0.4);
		cairo_fill (cr);
	} else if (_hovering) {
		Gtkmm2ext::rounded_rectangle (cr, CORNER_OFFSET, CORNER_OFFSET, w-CORNER_SIZE, h-CORNER_SIZE, CORNER_RADIUS);
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.1);
		cairo_fill (cr);
	}

	last_drawn = ds;

	return true;
}

void
PixFader::on_size_request (GtkRequisition* req)
{
	if (_orien == VERT) {
		req->width = (girth ? girth : -1);
		req->height = (span ? span : -1);
	} else {
		req->height = (girth ? girth : -1);
		req->width = (span ? span : -1);
	}
}

void
PixFader::on_size_allocate (Gtk::Allocation& alloc)
{
	DrawingArea::on_size_allocate(alloc);

	if (_orien == VERT) {
		girth = alloc.get_width ();
		span = alloc.get_height ();
	} else {
		girth = alloc.get_height ();
		span = alloc.get_width ();
	}

	update_unity_position ();
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
	gdk_pointer_grab(ev->window,false,
			GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
			NULL,NULL,ev->time);

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
			gdk_pointer_ungrab (GDK_CURRENT_TIME);

			if (!_hovering) {
				Keyboard::magic_widget_drop_focus();
				queue_draw ();
			}

			if (ev_pos == grab_start) {

				/* no motion - just a click */

				if (ev->state & Keyboard::TertiaryModifier) {
					adjustment.set_value (default_value);
				} else if (ev->state & Keyboard::GainFineScaleModifier) {
					adjustment.set_value (adjustment.get_lower());
				} else if ((_orien == VERT && ev_pos < display_span()) || (_orien == HORIZ && ev_pos > display_span())) {
					/* above the current display height, remember X Window coords */
					adjustment.set_value (adjustment.get_value() + adjustment.get_step_increment());
				} else {
					adjustment.set_value (adjustment.get_value() - adjustment.get_step_increment());
				}
			}
			return true;
		} 
		break;
		
	case 2:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			set_adjustment_from_event (ev);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			return true;
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
float
PixFader::display_span ()
{
	float fract = (adjustment.get_value () - adjustment.get_lower()) / ((adjustment.get_upper() - adjustment.get_lower()));
	
	return fract;
}

void
PixFader::update_unity_position ()
{
	if (_orien == VERT) {
		unity_loc = (int) rint (span * (1 - ((default_value - adjustment.get_lower()) / (adjustment.get_upper() - adjustment.get_lower())))) - 1;
	} else {
		unity_loc = (int) rint ((default_value - adjustment.get_lower()) * span / (adjustment.get_upper() - adjustment.get_lower()));
	}

	queue_draw ();
}

bool
PixFader::on_enter_notify_event (GdkEventCrossing*)
{
	_hovering = true;
	Keyboard::magic_widget_grab_focus ();
	queue_draw ();
	return false;
}

bool
PixFader::on_leave_notify_event (GdkEventCrossing*)
{
	if (!dragging) {
		_hovering = false;
		Keyboard::magic_widget_drop_focus();
		queue_draw ();
	}
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

void
PixFader::set_text (const std::string& str)
{
	_text = str;

 	if (!_layout && !_text.empty()) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	if (_layout) {
		_layout->set_text (str);
		_layout->get_pixel_size (_text_width, _text_height);
	}

	queue_resize ();
}

void
PixFader::on_state_changed (Gtk::StateType old_state)
{
	Widget::on_state_changed (old_state);
	queue_draw ();
}

void
PixFader::on_style_changed (const Glib::RefPtr<Gtk::Style>&)
{
	if (_layout) {
		std::string txt = _layout->get_text();
		_layout.clear (); // drop reference to existing layout
		set_text (txt);
	}

	queue_draw ();
}
