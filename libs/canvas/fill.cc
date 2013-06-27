/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include "ardour/utils.h"

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "canvas/fill.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

Fill::Fill (Group* parent)
	: Item (parent)
	, _fill_color (0x000000ff)
	, _fill (true)
{

}

void
Fill::set_fill_color (Color color)
{
	if (_fill_color != color) {
		begin_visual_change ();
		_fill_color = color;
		end_visual_change ();
	}
}

void
Fill::set_fill (bool fill)
{
	if (_fill != fill) {
		begin_visual_change ();
		_fill = fill;
		end_visual_change ();
	}
}

void
Fill::setup_fill_context (Cairo::RefPtr<Cairo::Context> context) const
{
	set_source_rgba (context, _fill_color);
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
		color_to_rgba (s->second, r, g, b, a);
		_gradient->add_color_stop_rgba (s->first, r, g, b, a);
	}
	
	context->set_source (_gradient);
}

void
Fill::set_gradient (StopList const & stops, bool vertical)
{
	begin_visual_change ();

	if (stops.empty()) {
		_stops.clear ();
	} else {
		_stops = stops;
		_vertical_gradient = vertical;
	}

	end_visual_change ();
}
