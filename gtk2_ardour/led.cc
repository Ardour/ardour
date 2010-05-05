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

LED::LED()
        : _visual_state (0)
        , _active (false)
        , _red (0.0)
        , _green (1.0)
        , _blue (0.0)

{
}

LED::~LED()
{
}

void
LED::render (cairo_t* cr)
{
        float diameter = std::min (_width, _height);

        //background
        cairo_rectangle(cr, 0, 0, _width, _height);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_fill(cr);

	cairo_translate(cr, _width/2, _height/2);

#if 0
	//inset
	cairo_pattern_t *pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, diameter);
	cairo_pattern_add_color_stop_rgba (pat, 0, 0,0,0, 0.4);
	cairo_pattern_add_color_stop_rgba (pat, 1, 1,1,1, 0.7);
	cairo_arc (cr, 0, 0, diameter/2, 0, 2 * M_PI);
	cairo_set_source (cr, pat);
	cairo_fill (cr);
	cairo_pattern_destroy (pat);

	//black ring
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_arc (cr, 0, 0, diameter/2-2, 0, 2 * M_PI);
	cairo_fill(cr);

	//knob color
	cairo_set_source_rgba (cr, _red, _green, _blue, _active ? 0.8 : 0.2);
	cairo_arc (cr, 0, 0, diameter/2-3, 0, 2 * M_PI);
	cairo_fill(cr);

	//reflection
	cairo_scale(cr, 0.7, 0.7);
	cairo_pattern_t *pat2 = cairo_pattern_create_linear (0.0, 0.0, 0.0, diameter/2-3);
	cairo_pattern_add_color_stop_rgba (pat2, 0, 1,1,1, _active ? 0.4 : 0.2);
	cairo_pattern_add_color_stop_rgba (pat2, 1, 1,1,1, 0.0);
	cairo_arc (cr, 0, 0, diameter/2-3, 0, 2 * M_PI);
	cairo_set_source (cr, pat2);
	cairo_fill (cr);
	cairo_pattern_destroy (pat2);
#endif

	cairo_set_source_rgba (cr, _red, _green, _blue,  1.0);
	cairo_arc (cr, 0, 0, diameter/2-5, 0, 2 * M_PI);
	cairo_fill(cr);

	cairo_stroke (cr);
}

void
LED::set_visual_state (int32_t s)
{
        if (s != _visual_state) {

                _visual_state = s;

                RefPtr<Style> style = get_style();
                Color c;
                
                switch (_visual_state) {
                case 0:
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
}
