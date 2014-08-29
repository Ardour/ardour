/*
    Copyright (C) 2010 Paul Davis

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

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "canvas/utils.h"

#include "ardour_button.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

ArdourButton::Element ArdourButton::default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text);
ArdourButton::Element ArdourButton::led_default_elements = ArdourButton::Element (ArdourButton::default_elements|ArdourButton::Indicator);
ArdourButton::Element ArdourButton::just_led_default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Indicator);

ArdourButton::ArdourButton (Element e)
	: _elements (e)
	, _tweaks (Tweaks (0))
	, _text_width (0)
	, _text_height (0)
	, _diameter (11.0)
	, _corner_radius (4.0)
	, _corner_mask (0xf)
	, _angle(0)
	, _xalign(.5)
	, _yalign(.5)
	, fill_inactive_color (0)
	, fill_active_color (0)
	, text_active_color(0)
	, text_inactive_color(0)
	, led_active_color(0)
	, led_inactive_color(0)
	, convex_pattern (0)
	, concave_pattern (0)
	, led_inset_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _led_left (false)
	, _fixed_diameter (true)
	, _distinct_led_click (false)
	, _hovering (false)
	, _focused (false)
	, _fixed_colors_set (false)
	, _fallthrough_to_parent (false)
{
	ARDOUR_UI_UTILS::ColorsChanged.connect (sigc::mem_fun (*this, &ArdourButton::color_handler));
}

ArdourButton::ArdourButton (const std::string& str, Element e)
	: _elements (e)
	, _tweaks (Tweaks (0))
	, _text_width (0)
	, _text_height (0)
	, _diameter (11.0)
	, _corner_radius (4.0)
	, _corner_mask (0xf)
	, _angle(0)
	, _xalign(.5)
	, _yalign(.5)
	, fill_inactive_color (0)
	, fill_active_color (0)
	, text_active_color(0)
	, text_inactive_color(0)
	, led_active_color(0)
	, led_inactive_color(0)
	, convex_pattern (0)
	, concave_pattern (0)
	, led_inset_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _led_left (false)
	, _fixed_diameter (true)
	, _distinct_led_click (false)
	, _hovering (false)
	, _focused (false)
	, _fixed_colors_set (false)
{
	set_text (str);
}

ArdourButton::~ArdourButton()
{
	delete _led_rect;

	if (convex_pattern) {
		cairo_pattern_destroy (convex_pattern);
	}
	
	if (concave_pattern) {
		cairo_pattern_destroy (concave_pattern);
	}
	
	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
	}

}

void
ArdourButton::set_text (const std::string& str)
{
	_text = str;

	if (!_layout && !_text.empty()) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	if (_layout) {
		_layout->set_text (str);
	}

	queue_resize ();
}

void
ArdourButton::set_markup (const std::string& str)
{
	_text = str;

	if (!_layout) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	_layout->set_markup (str);
	queue_resize ();
}

void
ArdourButton::set_angle (const double angle)
{
	_angle = angle;
}

void
ArdourButton::set_alignment (const float xa, const float ya)
{
	_xalign = xa;
	_yalign = ya;
}

void
ArdourButton::render (cairo_t* cr, cairo_rectangle_t *)
{
	uint32_t text_color;
	uint32_t led_color;
	if ( active_state() == Gtkmm2ext::ExplicitActive ) {
		text_color = text_active_color;
		led_color = led_active_color;
	} else {
		text_color = text_inactive_color;
		led_color = led_inactive_color;
	}
	
	void (*rounded_function)(cairo_t*, double, double, double, double, double);

	switch (_corner_mask) {
	case 0x1: /* upper left only */
		rounded_function = Gtkmm2ext::rounded_top_left_rectangle;
		break;
	case 0x2: /* upper right only */
		rounded_function = Gtkmm2ext::rounded_top_right_rectangle;
		break;
	case 0x3: /* upper only */
		rounded_function = Gtkmm2ext::rounded_top_rectangle;
		break;
		/* should really have functions for lower right, lower left,
		   lower only, but for now, we don't
		*/
	default:
		rounded_function = Gtkmm2ext::rounded_rectangle;
	}

	if (!_fixed_diameter) {
		_diameter = std::min (get_width(), get_height());
	}

#if 0 // clear background - "transparent" round corners
	/* Alas, neither get_style()->get_bg nor get_parent()->get_style()->get_bg
	 * does work for all places in ardour where a button is used.
	 * gtk style are sadly inconsisent throughout ardour.
	 * -> disabled for now.
	 */
	if (get_parent ()) {
		Gdk::Color c = get_parent ()->get_style ()->get_bg (get_parent ()->get_state ());
		CairoWidget::set_source_rgb_a (cr, c);
		cairo_rectangle (cr, 0, 0, get_width(), get_height());
		cairo_fill(cr);
	}
#endif
	
	// background fill
	if ((_elements & Body)==Body) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
		if (active_state() == Gtkmm2ext::ImplicitActive && !((_elements & Indicator)==Indicator)) {
			ArdourCanvas::set_source_rgba (cr, fill_inactive_color);
			cairo_fill (cr);
		} else if ( (active_state() == Gtkmm2ext::ExplicitActive) && !((_elements & Indicator)==Indicator) ) {
			//background color
			ArdourCanvas::set_source_rgba (cr, fill_active_color);
			cairo_fill (cr);
		} else {  //inactive, or it has an indicator
			//background color
			ArdourCanvas::set_source_rgba (cr, fill_inactive_color);
		}
		cairo_fill (cr);
	}

	//show the "convex" or "concave" gradient
	if (!_flat_buttons) {
		if ( active_state() == Gtkmm2ext::ExplicitActive && !((_elements & Indicator)==Indicator) ) {
			//concave
			cairo_set_source (cr, concave_pattern);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
			cairo_fill (cr);
		} else {
			cairo_set_source (cr, convex_pattern);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
			cairo_fill (cr);
		}
	}

	// indicator border on top of gradient
	if ((_elements & Body)==Body) {
		if (active_state() == Gtkmm2ext::ImplicitActive && !((_elements & Indicator)==Indicator)) {
			cairo_set_line_width (cr, 3.0);
			rounded_function (cr, 1.5, 1.5, get_width() - 3, get_height() - 3, _corner_radius);
			ArdourCanvas::set_source_rgba (cr, fill_active_color);
			cairo_stroke (cr);
		}
	}

	// draw edge
	if ((_elements & (Body|Edge)) == (Body|Edge)) {
		rounded_function (cr, .5, .5, get_width() - 1, get_height() - 1, _corner_radius);
		cairo_set_source_rgba (cr, 0, 0, 0, 1);
		cairo_set_line_width (cr, 1.0);
		cairo_stroke(cr);
	}

	//Pixbuf, if any
	if (_pixbuf) {

		double x,y;
		x = (get_width() - _pixbuf->get_width())/2.0;
		y = (get_height() - _pixbuf->get_height())/2.0;

		//if this is a DropDown with an icon, then we need to 
		  //move the icon left slightly to accomomodate the arrow
		if (((_elements & Menu)==Menu)) {
			cairo_save (cr);
			cairo_translate (cr, -8,0 );
		}
		
		cairo_rectangle (cr, x, y, _pixbuf->get_width(), _pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, _pixbuf->gobj(), x, y);
		cairo_fill (cr);

		//..and then return to our previous drawing position
		if (((_elements & Menu)==Menu)) {
			cairo_restore (cr);
		}
	}

	int text_margin;
	if (get_width() < 75) {
		text_margin = 5;
	} else {
		text_margin = 10;
	}

	// Text, if any
	if ( !_pixbuf && ((_elements & Text)==Text) && !_text.empty()) {

		cairo_save (cr);
		cairo_rectangle (cr, 2, 1, get_width()-4, get_height()-2);
		cairo_clip(cr);

		cairo_new_path (cr);	
		ArdourCanvas::set_source_rgba (cr, text_color);

		if ( (_elements & Menu) == Menu) {
			cairo_move_to (cr, text_margin, get_height()/2.0 - _text_height/2.0);
			pango_cairo_show_layout (cr, _layout->gobj());
		} else if ( (_elements & Indicator)  == Indicator) {
			if (_led_left) {
				cairo_move_to (cr, text_margin + _diameter + 4, get_height()/2.0 - _text_height/2.0);
			} else {
				cairo_move_to (cr, text_margin, get_height()/2.0 - _text_height/2.0);
			}
			pango_cairo_show_layout (cr, _layout->gobj());
		} else {
			/* align text */

			double ww, wh;
			double xa, ya;
			ww = get_width();
			wh = get_height();
			cairo_save (cr); // TODO retain rotataion.. adj. LED,...
			cairo_rotate(cr, _angle * M_PI / 180.0);
			cairo_device_to_user(cr, &ww, &wh);
			xa = (ww - _text_width) * _xalign;
			ya = (wh - _text_height) * _yalign;

			/* quick hack for left/bottom alignment at -90deg
			 * TODO this should be generalized incl rotation.
			 * currently only 'user' of this API is meter_strip.cc
			 */
			if (_xalign < 0) xa = (ww * fabs(_xalign) + text_margin);

			// TODO honor left/right text_margin with min/max()

			cairo_move_to (cr, xa, ya);
			pango_cairo_update_layout(cr, _layout->gobj());
			pango_cairo_show_layout (cr, _layout->gobj());
			cairo_restore (cr);

			/* use old center'ed layout for follow up items - until rotation/aligment code is completed */
			cairo_move_to (cr, (get_width() - _text_width)/2.0, get_height()/2.0 - _text_height/2.0);
		}
		cairo_restore (cr);
	} 

	//Menu "triangle"
	if (((_elements & Menu)==Menu)) {
	
		cairo_save (cr);

		//menu arrow
		cairo_set_source_rgba (cr, 1, 1, 1, 0.4);
		cairo_move_to(cr, get_width() - ((_diameter/2.0) + 6.0), get_height()/2.0 +_diameter/4);
		cairo_rel_line_to(cr, -_diameter/2, -_diameter/2);
		cairo_rel_line_to(cr, _diameter, 0);
		cairo_close_path(cr);
		cairo_set_source_rgba (cr, 1, 1, 1, 0.4);
		cairo_fill_preserve(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 0.8);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);
	
		cairo_restore (cr);
	}
	
	//Indicator LED
	if (((_elements & Indicator)==Indicator)) {

		/* move to the center of the indicator/led */

		cairo_save (cr);

		if (_elements & Text) {
			if (_led_left) {
				cairo_translate (cr, text_margin + (_diameter/2.0), get_height()/2.0);
			} else {
				cairo_translate (cr, get_width() - ((_diameter/2.0) + 4.0), get_height()/2.0);
			}
		} else {
			cairo_translate (cr, get_width()/2.0, get_height()/2.0);
		}
		
		//inset
		if (!_flat_buttons) {
			cairo_arc (cr, 0, 0, _diameter/2, 0, 2 * M_PI);
			cairo_set_source (cr, led_inset_pattern);
			cairo_fill (cr);
		}

		//black ring
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_arc (cr, 0, 0, _diameter/2-1, 0, 2 * M_PI);
		cairo_fill(cr);

		//led color
		ArdourCanvas::set_source_rgba (cr, led_color);
		cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
		cairo_fill(cr);
		
		cairo_restore (cr);
	}


	// a transparent gray layer to indicate insensitivity
	if ((visual_state() & Gtkmm2ext::Insensitive)) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
		cairo_set_source_rgba (cr, 0.505, 0.517, 0.525, 0.6);
		cairo_fill (cr);
	}

	// if requested, show hovering
	if (ARDOUR::Config->get_widget_prelight()
			&& !((visual_state() & Gtkmm2ext::Insensitive))) {
		if (_hovering) {
			rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
			cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.2);
			cairo_fill (cr);
		}
	}

	//user is currently pressing the button.  black outline helps to indicate this
	if ( _grabbed && !(_elements & (Inactive|Menu))) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, _corner_radius);
		cairo_set_line_width(cr, 2);
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, .5); // XXX no longer 'black'
		cairo_stroke (cr);
	}
	
	//some buttons (like processor boxes) can be selected  (so they can be deleted).  Draw a selection indicator
	if (visual_state() & Gtkmm2ext::Selected) {
#if 0 // outline, edge + 1px outside
		cairo_set_line_width(cr, 2);
		cairo_set_source_rgba (cr, 1, 0, 0, 0.8);
#else // replace edge (if any)
		cairo_set_line_width(cr, 1);
		cairo_set_source_rgba (cr, 1, 0, 0, 0.8);
#endif
		rounded_function (cr, 0.5, 0.5, get_width() - 1, get_height() - 1, _corner_radius);
		cairo_stroke (cr);
	}
	
	//I guess this means we have keyboard focus.  I don't think this works currently
	//
	//A: yes, it's keyboard focus and it does work when there's no editor window
	//   (the editor is always the first receiver for KeyDown).
	//   It's needed for eg. the engine-dialog at startup or after closing a sesion.
	if (_focused) {
		rounded_function (cr, 1.5, 1.5, get_width() - 3, get_height() - 3, _corner_radius);
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.8);
		double dashes = 1;
		cairo_set_dash (cr, &dashes, 1, 0);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_width (cr, 1.0);
		cairo_stroke (cr);
		cairo_set_dash (cr, 0, 0, 0);
	}
}

void
ArdourButton::set_diameter (float d)
{
	_diameter = (d*2) + 5.0;

	if (_diameter != 0.0) {
		_fixed_diameter = true;
	}

	build_patterns ();
	queue_resize ();
}

void
ArdourButton::set_corner_radius (float r)
{
	_corner_radius = r;
	set_dirty ();
}

void
ArdourButton::on_size_request (Gtk::Requisition* req)
{
	int xpad = 0;
	int ypad = 6;

	CairoWidget::on_size_request (req);

	if ((_elements & Text) && !_text.empty()) {

		//calc our real width for our string (but ignore the height, because that results in inconsistent button heights)
		int ignored;
		_layout->get_pixel_size (_text_width, ignored);

		//calc the height using some text with both ascenders and descenders
		std::string t = _layout->get_text();
		_layout->set_text ("WjgO");  //what we put here probably doesn't matter, as long as its the same for everyone
		_layout->get_pixel_size (ignored, _text_height);
		_layout->set_text (t);

		if (_text_width + _diameter < 75) {
			xpad = 7;
		} else {
			xpad = 12;
		}
	} else {
		_text_width = 0;
		_text_height = 0;
	}

	if (_pixbuf) {
		xpad = 6;
	}

        if ((_elements & Indicator) && _fixed_diameter) {
		if (_pixbuf) {
			req->width = _pixbuf->get_width() + lrint (_diameter) + xpad;
			req->height = max (_pixbuf->get_height(), (int) lrint (_diameter)) + ypad;
		} else {
			req->width = _text_width + lrint (_diameter) + xpad * 2; // margin left+right * 2
			req->height = max (_text_height, (int) lrint (_diameter)) + ypad;
		}
        } else {
		if (_pixbuf) {
			req->width = _pixbuf->get_width() + xpad;
			req->height = _pixbuf->get_height() + ypad;
		}  else {
			req->width = _text_width + xpad;
			req->height = _text_height + ypad;
		}
	}
	req->width += _corner_radius;
}

/**
 * This sets the colors used for rendering based on the name of the button, and
 * thus uses information from the GUI config data. 
 */
void
ArdourButton::set_colors ()
{
	if (_fixed_colors_set) {
		return;
	}
	std::string name = get_name();

	fill_active_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill active", name));
	fill_inactive_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill", name));

	text_active_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: text active", name));
	text_inactive_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: text", name));

	led_active_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: led active", name));
	led_inactive_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: led", name));
}
/**
 * This sets the colors used for rendering based on two fixed values, rather
 * than basing them on the button name, and thus information in the GUI config
 * data.
 */
void ArdourButton::set_fixed_colors (const uint32_t color_active, const uint32_t color_inactive)
{
	_fixed_colors_set = true;

	fill_active_color = color_active;
	fill_inactive_color = color_inactive;

	unsigned char r, g, b, a;
	UINT_TO_RGBA(color_active, &r, &g, &b, &a);
	
	double white_contrast = (max (double(r), 255.) - min (double(r), 255.)) +
		(max (double(g), 255.) - min (double(g), 255.)) +
		(max (double(b), 255.) - min (double(b), 255.));
	
	double black_contrast = (max (double(r), 0.) - min (double(r), 0.)) +
		(max (double(g), 0.) - min (double(g), 0.)) +
		(max (double(b), 0.) - min (double(b), 0.));
	
	text_active_color = (white_contrast > black_contrast) ?
		RGBA_TO_UINT(255, 255, 255, 255) : /* use white */
		RGBA_TO_UINT(  0,   0,   0,   255);  /* use black */
	

	UINT_TO_RGBA(color_inactive, &r, &g, &b, &a);

	white_contrast = (max (double(r), 255.) - min (double(r), 255.)) +
		(max (double(g), 255.) - min (double(g), 255.)) +
		(max (double(b), 255.) - min (double(b), 255.));
	
	black_contrast = (max (double(r), 0.) - min (double(r), 0.)) +
		(max (double(g), 0.) - min (double(g), 0.)) +
		(max (double(b), 0.) - min (double(b), 0.));
	
	text_inactive_color = (white_contrast > black_contrast) ?
		RGBA_TO_UINT(255, 255, 255, 255) : /* use white */
		RGBA_TO_UINT(  0,   0,   0,   255);  /* use black */
	
	/* XXX what about led colors ? */

	build_patterns ();
	set_name (""); /* this will trigger a "style-changed" message and then set_colors() */
}

void
ArdourButton::build_patterns ()
{
	if (convex_pattern) {
		cairo_pattern_destroy (convex_pattern);
		convex_pattern = 0;
	}

	if (concave_pattern) {
		cairo_pattern_destroy (concave_pattern);
		concave_pattern = 0;
	}

	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
	}
	
	float width = get_width();  float height = get_height();

	//convex gradient
	convex_pattern = cairo_pattern_create_linear (0.0, 0, 0.0,  height);
	cairo_pattern_add_color_stop_rgba (convex_pattern, 0.0, 0,0,0, 0.0);
	cairo_pattern_add_color_stop_rgba (convex_pattern, 1.0, 0,0,0, 0.35);

	//concave gradient
	concave_pattern = cairo_pattern_create_linear (0.0, 0, 0.0,  height);
	cairo_pattern_add_color_stop_rgba (concave_pattern, 0.0, 0,0,0, 0.5);
	cairo_pattern_add_color_stop_rgba (concave_pattern, 0.7, 0,0,0, 0.0);

	if (_elements & Indicator) {
		led_inset_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter);
		cairo_pattern_add_color_stop_rgba (led_inset_pattern, 0, 0,0,0, 0.4);
		cairo_pattern_add_color_stop_rgba (led_inset_pattern, 1, 1,1,1, 0.7);
	}

	set_dirty ();
}

void
ArdourButton::set_led_left (bool yn)
{
	_led_left = yn;
}

bool
ArdourButton::on_button_press_event (GdkEventButton *ev)
{
	if ((_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width && 
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			return true;
		}
	}

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}
	
	_grabbed = true;
	queue_draw ();

	if (!_act_on_release) {
		if (_action) {
			_action->activate ();
			return true;
		}
	}

	if (_fallthrough_to_parent)
		return false;

	return true;
}

bool
ArdourButton::on_button_release_event (GdkEventButton *ev)
{
	if (_hovering && (_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width && 
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			signal_led_clicked(); /* EMIT SIGNAL */
			return true;
		}
	}

	_grabbed = false;
	queue_draw ();

	if (_hovering) {
		signal_clicked ();	
		if (_act_on_release) {
			if (_action) {
				_action->activate ();
				return true;
			}
		}
	}
	
	if (_fallthrough_to_parent)
		return false;

	return true;
}

void
ArdourButton::set_distinct_led_click (bool yn)
{
	_distinct_led_click = yn;
	setup_led_rect ();
}

void
ArdourButton::color_handler ()
{
	set_colors ();
	build_patterns ();
	set_dirty ();
}

void
ArdourButton::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	setup_led_rect ();
	build_patterns ();
}

void
ArdourButton::set_controllable (boost::shared_ptr<Controllable> c)
{
        watch_connection.disconnect ();
        binding_proxy.set_controllable (c);
}

void
ArdourButton::watch ()
{
        boost::shared_ptr<Controllable> c (binding_proxy.get_controllable ());

        if (!c) {
                warning << _("button cannot watch state of non-existing Controllable\n") << endmsg;
                return;
        }

        c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourButton::controllable_changed, this), gui_context());
}

void
ArdourButton::controllable_changed ()
{
        float val = binding_proxy.get_controllable()->get_value();

	if (fabs (val) >= 0.5f) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		unset_active_state ();
	}
}

void
ArdourButton::set_related_action (RefPtr<Action> act)
{
	Gtkmm2ext::Activatable::set_related_action (act);

	if (_action) {

		action_tooltip_changed ();

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);
		if (tact) {
			action_toggled ();
			tact->signal_toggled().connect (sigc::mem_fun (*this, &ArdourButton::action_toggled));
		} 

		_action->connect_property_changed ("sensitive", sigc::mem_fun (*this, &ArdourButton::action_sensitivity_changed));
		_action->connect_property_changed ("visible", sigc::mem_fun (*this, &ArdourButton::action_visibility_changed));
		_action->connect_property_changed ("tooltip", sigc::mem_fun (*this, &ArdourButton::action_tooltip_changed));
	}
}

void
ArdourButton::action_toggled ()
{
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);

	if (tact) {
		if (tact->get_active()) {
			set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			unset_active_state ();
		}
	}
}	

void
ArdourButton::on_style_changed (const RefPtr<Gtk::Style>&)
{
	set_colors ();
	build_patterns ();
}

void
ArdourButton::on_name_changed ()
{
	set_colors ();
	build_patterns ();
}

void
ArdourButton::setup_led_rect ()
{
	int text_margin;

	if (get_width() < 75) {
		text_margin = 3;
	} else {
		text_margin = 10;
	}

	if (_elements & Indicator) {
		_led_rect = new cairo_rectangle_t;
		
		if (_elements & Text) {
			if (_led_left) {
				_led_rect->x = text_margin;
			} else {
				_led_rect->x = get_width() - text_margin - _diameter/2.0;
			}
		} else {
			/* centered */
			_led_rect->x = get_width()/2.0 - _diameter/2.0;
		}

		_led_rect->y = get_height()/2.0 - _diameter/2.0;
		_led_rect->width = _diameter;
		_led_rect->height = _diameter;

	} else {
		delete _led_rect;
		_led_rect = 0;
	}
}

void
ArdourButton::set_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_pixbuf = img;
	queue_draw ();
}

void
ArdourButton::set_active_state (Gtkmm2ext::ActiveState s)
{
	bool changed = (_active_state != s);
	CairoWidget::set_active_state (s);
	if (changed) {
		set_colors ();
		build_patterns ();
	}
}
	
void
ArdourButton::set_visual_state (Gtkmm2ext::VisualState s)
{
	bool changed = (_visual_state != s);
	CairoWidget::set_visual_state (s);
	if (changed) {
		set_colors ();
		build_patterns ();
	}
}
	

bool
ArdourButton::on_focus_in_event (GdkEventFocus* ev)
{
	_focused = true;
	queue_draw ();
	return CairoWidget::on_focus_in_event (ev);
}

bool
ArdourButton::on_focus_out_event (GdkEventFocus* ev)
{
	_focused = false;
	queue_draw ();
	return CairoWidget::on_focus_out_event (ev);
}

bool
ArdourButton::on_key_release_event (GdkEventKey *ev) {
	if (_focused &&
			(ev->keyval == GDK_KEY_space || ev->keyval == GDK_Return))
	{
		signal_clicked();
		if (_action) {
			_action->activate ();
		}
		return true;
	}
	return CairoWidget::on_key_release_event (ev);
}

bool
ArdourButton::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = (_elements & Inactive) ? false : true;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ArdourButton::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	if (ARDOUR::Config->get_widget_prelight()) {
		queue_draw ();
	}

	return CairoWidget::on_leave_notify_event (ev);
}

void
ArdourButton::set_tweaks (Tweaks t)
{
	if (_tweaks != t) {
		_tweaks = t;
		queue_draw ();
	}
}

void
ArdourButton::action_sensitivity_changed ()
{
	if (_action->property_sensitive ()) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	}
	
}


void
ArdourButton::action_visibility_changed ()
{
	if (_action->property_visible ()) {
		show ();
	} else {
		hide ();
	}
}

void
ArdourButton::action_tooltip_changed ()
{
	string str = _action->property_tooltip().get_value();
	ARDOUR_UI::instance()->set_tip (*this, str);
}

void
ArdourButton::set_elements (Element e)
{
	_elements = e;
}

void
ArdourButton::add_elements (Element e)
{
	_elements = (ArdourButton::Element) (_elements | e);
}
