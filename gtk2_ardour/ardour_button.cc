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

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour_button.h"
#include "ardour_ui.h"
#include "global_signals.h"

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using std::max;
using std::min;
using std::cerr;
using std::endl;

ArdourButton::ArdourButton()
        : _text_width (0)
	, _text_height (0)
	, _led_left (false)
        , _diameter (0.0)
        , _fixed_diameter (false)
	, _distinct_led_click (true)
	, edge_pattern (0)
	, fill_pattern (0)
	, led_inset_pattern (0)
	, reflection_pattern (0)
	  
{
	ColorsChanged.connect (sigc::mem_fun (*this, &ArdourButton::color_handler));
}

ArdourButton::~ArdourButton()
{
}

void
ArdourButton::set_text (const std::string& str)
{
	_text = str;

	if (!_layout && !_text.empty()) {
		_layout = Pango::Layout::create (get_pango_context());
	} 
	
	_layout->set_text (str);

	queue_resize ();
}

void
ArdourButton::set_markup (const std::string& str)
{
	_text = str;

	if (!_layout) {
		_layout = Pango::Layout::create (get_pango_context());
	} 

	_layout->set_text (str);
	queue_resize ();
}

void
ArdourButton::render (cairo_t* cr)
{
        if (!_fixed_diameter) {
                _diameter = std::min (_width, _height);
        }

	/* background fill. use parent window style, so that we fit in nicely.
	 */
	
	Color c = get_parent_bg ();
	
        cairo_rectangle (cr, 0, 0, _width, _height);
        cairo_stroke_preserve (cr);
        cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
        cairo_fill (cr);

	/* edge */

	Gtkmm2ext::rounded_rectangle (cr, 0, 0, _width, _height, 9);
	cairo_set_source (cr, edge_pattern);
	cairo_fill (cr);

	/*  button itself: leaves 1 pixel border of the edge visible all around. */

	Gtkmm2ext::rounded_rectangle (cr, 1, 1, _width-2, _height-2, 9);
	cairo_set_source (cr, fill_pattern);
	cairo_fill (cr);

	/* text, if any */

	if (!_text.empty()) {
		cairo_set_source_rgba (cr, text_r, text_g, text_b, text_a);
		if (_led_left) {
			cairo_move_to (cr, _diameter + 3 + 4, _height/2.0 - _text_height/2.0);
		} else {
			cairo_move_to (cr, 3, _height/2.0 - _text_height/2.0);
		}
		pango_cairo_show_layout (cr, _layout->gobj());
	}

	/* move to the center of the ArdourButton itself */

	if (_led_left) {
		cairo_translate (cr, 3 + (_diameter/2.0), _height/2.0);
	} else {
		cairo_translate (cr, _width - ((_diameter/2.0) + 4.0), _height/2.0);
	}

	//inset
	cairo_arc (cr, 0, 0, _diameter/2, 0, 2 * M_PI);
	cairo_set_source (cr, led_inset_pattern);
	cairo_fill (cr);

	//black ring
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_arc (cr, 0, 0, _diameter/2-2, 0, 2 * M_PI);
	cairo_fill(cr);

	//led color
	cairo_set_source_rgba (cr, led_r, led_g, led_b, led_a);
	cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
	cairo_fill(cr);

	//reflection
	cairo_scale(cr, 0.7, 0.7);
	cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
	cairo_set_source (cr, reflection_pattern);
	cairo_fill (cr);
	cairo_stroke (cr); // ??
}

void
ArdourButton::set_state (CairoWidget::State s, bool yn)
{
	CairoWidget::set_state (s, yn);
	set_colors ();
}

void
ArdourButton::set_diameter (float d)
{
        _diameter = (d*2) + 5.0;

        if (_diameter != 0.0) {
                _fixed_diameter = true;
        }

        set_dirty ();
}

void
ArdourButton::on_realize ()
{
        set_colors ();
        CairoWidget::on_realize ();
}

void
ArdourButton::on_size_request (Gtk::Requisition* req)
{
	int xpad = 0;
	int ypad = 6;

	if (!_text.empty()) {
		_layout->get_pixel_size (_text_width, _text_height);
		xpad += 6;
	}
		
        if (_fixed_diameter) {
                req->width = _text_width + (int) _diameter + xpad;
                req->height = max (_text_height, (int) _diameter) + ypad;
        } else {
                CairoWidget::on_size_request (req);
        }
}

void
ArdourButton::set_colors ()
{
	uint32_t start_color;
	uint32_t end_color;
	uint32_t r, g, b, a;
	uint32_t text_color;
	uint32_t led_color;

	/* we use the edge of the button to show Selected state, so the
	 * color/pattern used there will vary depending on that
	 */
	
	if (edge_pattern) {
		cairo_pattern_destroy (edge_pattern);
	}

	edge_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _height);
	if (_state & CairoWidget::Selected) {
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 border start selected", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 border end selected", get_name()));
	} else {
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 border start", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 border end", get_name()));
	}
	UINT_TO_RGBA (start_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (edge_pattern, 0, r/255.0,g/255.0,b/255.0, 0.7);
	UINT_TO_RGBA (end_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (edge_pattern, 1, r/255.0,g/255.0,b/255.0, 0.7);

	/* the fill pattern is used to indicate Normal/Active/Mid state
	 */

	if (fill_pattern) {
		cairo_pattern_destroy (fill_pattern);
	}

	fill_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _height);
	if (_state & Mid) {
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill start mid", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill end mid", get_name()));
	} else if (_state & Active) {
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill start active", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill end active", get_name()));
	} else {
		start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill start", get_name()));
		end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 fill end", get_name()));
	}
	UINT_TO_RGBA (start_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (fill_pattern, 0, r/255.0,g/255.0,b/255.0, a/255.0);
	UINT_TO_RGBA (end_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (fill_pattern, 1, r/255.0,g/255.0,b/255.0, a/255.0);

	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
	}

	led_inset_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter);
	cairo_pattern_add_color_stop_rgba (led_inset_pattern, 0, 0,0,0, 0.4);
	cairo_pattern_add_color_stop_rgba (led_inset_pattern, 1, 1,1,1, 0.7);
	
	if (reflection_pattern) {
		cairo_pattern_destroy (reflection_pattern);
	}

	reflection_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter/2-3);
	cairo_pattern_add_color_stop_rgba (reflection_pattern, 0, 1,1,1, (_state & Active) ? 0.4 : 0.2);
	cairo_pattern_add_color_stop_rgba (reflection_pattern, 1, 1,1,1, 0.0);

	/* text and LED colors depend on Active/Normal/Mid */
	if (_state & Active) {
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 text active", get_name()));
		led_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 led active", get_name()));
	} else if (_state & Mid) {
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 text mid", get_name()));
		led_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 led active", get_name()));
	} else {
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 text", get_name()));
		led_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 led", get_name()));
	}

	UINT_TO_RGBA (text_color, &r, &g, &b, &a);
	text_r = r/255.0;
	text_g = g/255.0;
	text_b = b/255.0;
	text_a = a/255.0;
	UINT_TO_RGBA (led_color, &r, &g, &b, &a);
	led_r = r/255.0;
	led_g = g/255.0;
	led_b = b/255.0;
	led_a = a/255.0;

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
	if (_distinct_led_click) {
		/* if within LED, swallow event */
		
		int top = lrint (_height/2.0 - _diameter/2.0);
		int bottom = lrint (_height/2.0 + _diameter/2.0);
		int left;
		int right;
		
		if (_led_left) {
			left = 4;
			right = left + _diameter;
		} else {
			left = lrint (_width - 4 - _diameter/2.0);
			right = left + _diameter;
		}

		if (ev->x >= left && ev->x <= right && ev->y <= bottom && ev->y >= top) {
			return true;
		}
	}

	return false;
}

bool
ArdourButton::on_button_release_event (GdkEventButton *ev)
{

	if (_distinct_led_click) {

		/* if within LED, emit signal */

		int top = lrint (_height/2.0 - _diameter/2.0);
		int bottom = lrint (_height/2.0 + _diameter/2.0);
		int left;
		int right;
		if (_led_left) {
			left = 4;
			right = left + _diameter;
		} else {
			left = lrint (_width - 4 - _diameter/2.0);
			right = left + _diameter;
		}
		
		if (ev->x >= left && ev->x <= right && ev->y <= bottom && ev->y >= top) {
			signal_clicked(); /* EMIT SIGNAL */
			return true;
		}
	}

	return false;
}

void
ArdourButton::set_distinct_led_click (bool yn)
{
	_distinct_led_click = yn;
}

void
ArdourButton::color_handler ()
{
	set_colors ();
	set_dirty ();
}
