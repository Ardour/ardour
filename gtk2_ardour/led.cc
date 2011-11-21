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

#include "led.h"

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

LED::LED()
	: _diameter (0.0)
	, _red (0.0)
	, _green (1.0)
	, _blue (0.0)
	, _fixed_diameter (false)
{
}

LED::~LED()
{
}

void
LED::render (cairo_t* cr)
{
	if (!_fixed_diameter) {
		_diameter = std::min (get_width(), get_height());
	}

	//background

	Widget* parent;
	RefPtr<Style> style;
	Color c;

	parent = get_parent ();

	while (parent && !parent->get_has_window()) {
		parent = parent->get_parent();
	}

	if (parent && parent->get_has_window()) {
		style = parent->get_style ();
		c = style->get_bg (parent->get_state());
	} else {
		style = get_style ();
		c = style->get_bg (get_state());
	}

#if 0
	cairo_rectangle(cr, 0, 0, _width, _height);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	cairo_fill(cr);
#endif

	cairo_translate(cr, get_width()/2, get_height()/2);

	//inset
	cairo_pattern_t *pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter);
	cairo_pattern_add_color_stop_rgba (pat, 0, 0,0,0, 0.4);
	cairo_pattern_add_color_stop_rgba (pat, 1, 1,1,1, 0.7);
	cairo_arc (cr, 0, 0, _diameter/2, 0, 2 * M_PI);
	cairo_set_source (cr, pat);
	cairo_fill (cr);
	cairo_pattern_destroy (pat);

	//black ring
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_arc (cr, 0, 0, _diameter/2-2, 0, 2 * M_PI);
	cairo_fill(cr);

	//knob color
	cairo_set_source_rgba (cr, _red, _green, _blue, (active_state() == Active) ? 0.8 : 0.2);
	cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
	cairo_fill(cr);

	//reflection
	cairo_scale(cr, 0.7, 0.7);
	cairo_pattern_t *pat2 = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter/2-3);
	cairo_pattern_add_color_stop_rgba (pat2, 0, 1,1,1, active_state() ? 0.4 : 0.2);
	cairo_pattern_add_color_stop_rgba (pat2, 1, 1,1,1, 0.0);
	cairo_arc (cr, 0, 0, _diameter/2-3, 0, 2 * M_PI);
	cairo_set_source (cr, pat2);
	cairo_fill (cr);
	cairo_pattern_destroy (pat2);

	cairo_stroke (cr);
}

void
LED::set_diameter (float d)
{
	_diameter = (d*2) + 5.0;

	if (_diameter != 0.0) {
		_fixed_diameter = true;
	}

	set_dirty ();
}

void
LED::on_realize ()
{
	set_colors_from_style ();
	CairoWidget::on_realize ();
}

void
LED::on_size_request (Gtk::Requisition* req)
{
	if (_fixed_diameter) {
		req->width = _diameter;
		req->height = _diameter;
	} else {
		CairoWidget::on_size_request (req);
	}
}

void
LED::set_colors_from_style ()
{
	RefPtr<Style> style = get_style();
	Color c;

	switch (_visual_state) {
	case VisualState(0):
		c = style->get_fg (STATE_NORMAL);
		break;
	default:
		c = style->get_fg (STATE_ACTIVE);
		break;
	}

	_red = c.get_red_p ();
	_green = c.get_green_p ();
	_blue = c.get_blue_p ();

	set_dirty ();
}
