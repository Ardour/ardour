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

#include "canvas/polygon.h"

using namespace ArdourCanvas;

Polygon::Polygon (Group* parent)
	: Item (parent)
	, PolyItem (parent)
	, Fill (parent)
{

}

void
Polygon::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		
		if (!_points.empty ()) {
			context->move_to (_points.front().x, _points.front().y);
		}

		context->stroke_preserve ();
	}

	if (_fill) {
		setup_fill_context (context);
		context->fill ();
	}
}

