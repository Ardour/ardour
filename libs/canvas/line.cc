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
#include <cairomm/context.h>
#include "pbd/compose.h"
#include "canvas/line.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Line::Line (Group* parent)
	: Item (parent)
	, Outline (parent)
{

}

void
Line::compute_bounding_box () const
{
	Rect bbox;
	
	bbox.x0 = min (_points[0].x, _points[1].x);
	bbox.y0 = min (_points[0].y, _points[1].y);
	bbox.x1 = max (_points[0].x, _points[1].x);
	bbox.y1 = max (_points[0].y, _points[1].y);

	bbox = bbox.expand (_outline_width / 2);

	_bounding_box = bbox;
	_bounding_box_dirty = false;
}

void
Line::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	setup_outline_context (context);
	context->move_to (_points[0].x, _points[0].y);
	context->line_to (_points[1].x, _points[1].y);
	context->stroke ();
}

void
Line::set (Duple a, Duple b)
{
	begin_change ();

	_points[0] = a;
	_points[1] = b;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}

void
Line::set_x0 (Coord x0)
{
	begin_change ();
	
	_points[0].x = x0;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}

void
Line::set_y0 (Coord y0)
{
	begin_change ();

	_points[0].y = y0;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}

void
Line::set_x1 (Coord x1)
{
	begin_change ();

	_points[1].x = x1;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}

void
Line::set_y1 (Coord y1)
{
	begin_change ();

	_points[1].y = y1;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}
