/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cairomm/cairomm.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "canvas/fill.h"
#include "canvas/item.h"
#include "canvas/types.h"

using namespace std;
using namespace ArdourCanvas;

Fill::Fill (Item& self)
	: _self (self)
	, _fill_color (0x000000ff)
	, _fill (true)
	, _transparent (false)
{
}

void
Fill::set_fill_color (Gtkmm2ext::Color color)
{
	if (_fill_color != color) {
		_self.begin_visual_change ();
		_fill_color = color;

		double r, g, b, a;
		Gtkmm2ext::color_to_rgba (color, r, g, b, a);
		if (a == 0.0) {
			_transparent = true;
		} else {
			_transparent = false;
		}

		_self.end_visual_change ();
	}
}

void
Fill::set_fill (bool fill)
{
	if (_fill != fill) {
		_self.begin_visual_change ();
		_fill = fill;
		_self.end_visual_change ();
	}
}

void
Fill::setup_fill_context (Cairo::RefPtr<Cairo::Context> context) const
{
        if (_pattern) {
                context->set_source (_pattern);
        } else {
	        Gtkmm2ext::set_source_rgba (context, _fill_color);
        }
}

void
Fill::setup_gradient_context (Cairo::RefPtr<Cairo::Context> context, Rect const & self, Duple const & draw_origin) const
{
	Cairo::RefPtr<Cairo::LinearGradient> _gradient;

	if (_vertical_gradient) {
		_gradient = Cairo::LinearGradient::create (draw_origin.x, self.y0, draw_origin.x, self.y1);
	} else {
		_gradient = Cairo::LinearGradient::create (self.x0, draw_origin.y, self.x1, draw_origin.y);
	}

	for (StopList::const_iterator s = _stops.begin(); s != _stops.end(); ++s) {
		double r, g, b, a;
		Gtkmm2ext::color_to_rgba (s->second, r, g, b, a);
		_gradient->add_color_stop_rgba (s->first, r, g, b, a);
	}

	context->set_source (_gradient);
}

void
Fill::set_pattern (Cairo::RefPtr<Cairo::Pattern> p)
{
        _pattern = p;
}

void
Fill::set_gradient (StopList const & stops, bool vertical)
{
	_self.begin_visual_change ();

	if (stops.empty()) {
		_stops.clear ();
	} else {
		_stops = stops;
		_vertical_gradient = vertical;
	}

	_self.end_visual_change ();
}
