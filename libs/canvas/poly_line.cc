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

#include <algorithm>

#include "canvas/poly_line.h"
#include "canvas/canvas.h"
#include "canvas/utils.h"

using namespace ArdourCanvas;

PolyLine::PolyLine (Canvas* c)
	: PolyItem (c)
	, _threshold (1.0)
{
}

PolyLine::PolyLine (Item* parent)
	: PolyItem (parent)
	, _threshold (1.0)
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

void
PolyLine::set_steps (Points const & points, bool stepped)
{
	if (!stepped) {
		PolyItem::set(points);
		return;
	}

	Points copy;
	for (Points::const_iterator p = points.begin(); p != points.end();) {
		Points::const_iterator next = p;
		++next;

		copy.push_back(*p);
		if (next != points.end() && next->x != p->x) {
			copy.push_back(Duple(next->x, p->y));
		}

		p = next;
	}

	PolyItem::set(copy);
}

bool
PolyLine::covers (Duple const & point) const
{
	Duple p = window_to_item (point);

	const Points::size_type npoints = _points.size();
	
	if (npoints < 2) {
		return false;
	}

	Points::size_type i;
	Points::size_type j;

	/* repeat for each line segment */

	const Rect visible (window_to_item (_canvas->visible_area()));

	for (i = 1, j = 0; i < npoints; ++i, ++j) {

		Duple at;
		double t;
		Duple a (_points[j]);
		Duple b (_points[i]);
		
		/*
		  Clamp the line endpoints to the visible area of the canvas. If we do
		  not do this, we may have a line segment extending to COORD_MAX and our
		  math goes wrong.
		*/
		
		a.x = std::min (a.x, visible.x1);
		a.y = std::min (a.y, visible.y1);
		b.x = std::min (b.x, visible.x1);
		b.y = std::min (b.y, visible.y1);
		
		double d = distance_to_segment_squared (p, a, b, t, at);
		
		if (t < 0.0 || t > 1.0) {
			continue;
		}
		
		if (d < _threshold + _outline_width) {
			return true;
		}
		
	}
	
	return false;
}

void
PolyLine::set_covers_threshold (double t)
{
	_threshold = t;
}
