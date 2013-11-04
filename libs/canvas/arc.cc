/*
    Copyright (C) 2013 Paul Davis

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

#include <cmath>
#include <algorithm>

#include <cairomm/context.h>

#include "pbd/compose.h"
#include "canvas/circle.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Arc::Arc (Group* parent)
	: Item (parent)
	, Outline (parent)
	, Fill (parent)
	, _radius (0.0)
	, _arc_degrees (0.0)
	, _start_degrees (0.0)
{

}

void
Arc::compute_bounding_box () const
{
	Rect bbox;

	/* this could be smaller in the case of small _arc values
	   but I can't be bothered to optimize it.
	*/
	
	bbox.x0 = _center.x - _radius;
	bbox.y0 = _center.y - _radius;
	bbox.x1 = _center.x + _radius;
	bbox.y1 = _center.y + _radius;

	bbox = bbox.expand (0.5 + (_outline_width / 2));

	_bounding_box = bbox;
	_bounding_box_dirty = false;
}

void
Arc::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_radius <= 0.0 || _arc_degrees <= 0.0) {
		return;
	}
	
	Duple c = item_to_window (Duple (_center.x, _center.y));

	context->arc (c.x, c.y, _radius, _start_degrees * (M_PI/180.0), _arc_degrees * (M_PI/180.0));
	setup_fill_context (context);
	context->fill_preserve ();
	setup_outline_context (context);
	context->stroke ();
}

void
Arc::set_center (Duple const & c)
{
	begin_change ();

	_center = c;

	_bounding_box_dirty = true;
	end_change ();
}

void
Arc::set_radius (Coord r)
{
	begin_change ();
	
	_radius = r;

	_bounding_box_dirty = true;
	end_change ();
}	

void
Arc::set_arc (double deg)
{
	begin_change ();
	
	_arc_degrees = deg;

	_bounding_box_dirty = true;
	end_change ();
}	


void
Arc::set_start (double deg)
{
	begin_change ();
	
	_start_degrees = deg;
	
	_bounding_box_dirty = true;
	end_change ();
}	

bool
Arc::covers (Duple const & point) const
{
	Duple p = canvas_to_item (point);

	double angle_degs = atan (p.y/p.x) * 2.0 * M_PI;
	double radius = sqrt (p.x * p.x + p.y * p.y);
	
	return (angle_degs >= _start_degrees) && 
		(angle_degs <= (_start_degrees + _arc_degrees)) && 
		(radius < _radius);
}
