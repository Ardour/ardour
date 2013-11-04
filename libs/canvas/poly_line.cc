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

#include "canvas/poly_line.h"

using namespace ArdourCanvas;

PolyLine::PolyLine (Group* parent)
	: Item (parent)
	, PolyItem (parent)
{

}

void
PolyLine::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		context->stroke ();
	}
}

bool
PolyLine::covers (Duple const & point) const
{
	Duple p = canvas_to_item (point);

	const Points::size_type npoints = _points.size();
	
	if (npoints < 2) {
		return false;
	}

	Points::size_type i;
	Points::size_type j;

	/* repeat for each line segment */
	
	for (i = 1, j = 0; i < npoints; ++i, ++j) {

		/* compute area of triangle computed by the two line points and the one
		   we are being asked about. If zero (within a given tolerance), the
		   points are co-linear and the argument is on the line.
		*/

		double area = fabs (_points[j].x * (_points[j].y - p.y)) + 
  			           (_points[i].x * (p.y - _points[j].y)) + 
			           (p.x * (_points[j].y - _points[i].y));
		if (area < 0.001) {
			return true;
		}
	}

	return false;
}
