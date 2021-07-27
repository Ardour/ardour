/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

Line::Line (Canvas* c)
	: Item (c)
{
}

Line::Line (Item* parent)
	: Item (parent)
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

	bbox = bbox.expand (0.5 + (_outline_width / 2));

	_bounding_box = bbox;
	bb_clean ();
}

void
Line::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	setup_outline_context (context);

	Duple p0 = item_to_window (Duple (_points[0].x, _points[0].y));
	Duple p1 = item_to_window (Duple (_points[1].x, _points[1].y));

	if (_outline_width <= 1.0) {
		/* See Cairo FAQ on single pixel lines to understand why we add 0.5
		 */

		const Duple half_a_pixel (0.5, 0.5);
		p0 = p0.translate (half_a_pixel);
		p1 = p1.translate (half_a_pixel);
	}

	context->move_to (p0.x, p0.y);
	context->line_to (p1.x, p1.y);
	context->stroke ();
}

void
Line::set (Duple a, Duple b)
{
	if (a != _points[0] || b != _points[1]) {
		begin_change ();

		_points[0] = a;
		_points[1] = b;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Line::set_x (Coord x0, Coord x1)
{
	if (x0 != _points[0].x || x1 != _points[1].x) {
		begin_change ();

		_points[0].x = x0;
		_points[1].x = x1;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Line::set_x0 (Coord x0)
{
	if (x0 != _points[0].x) {
		begin_change ();

		_points[0].x = x0;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Line::set_y0 (Coord y0)
{
	if (y0 != _points[0].y) {
		begin_change ();

		_points[0].y = y0;

		_bounding_box_dirty = true;
		end_change ();
	}

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: line change\n");
}

void
Line::set_x1 (Coord x1)
{
	if (x1 != _points[1].x) {
		begin_change ();

		_points[1].x = x1;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Line::set_y1 (Coord y1)
{
	if (y1 != _points[1].y) {
		begin_change ();

		_points[1].y = y1;

		_bounding_box_dirty = true;
		end_change ();
	}
}

bool
Line::covers (Duple const & point) const
{
	const Duple p = window_to_item (point);
	static const Distance threshold = 2.0;

	/* this quick check works for vertical and horizontal lines, which are
	 * common.
	 */

	if (_points[0].x == _points[1].x) {
		/* line is vertical, just check x coordinate */
		return fabs (_points[0].x - p.x) <= threshold;
	}

	if (_points[0].y == _points[1].y) {
		/* line is horizontal, just check y coordinate */
		return fabs (_points[0].y - p.y) <= threshold;
	}

	Duple at;
	double t;
	Duple a (_points[0]);
	Duple b (_points[1]);
	const Rect visible (window_to_item (_canvas->visible_area()));

	/*
	   Clamp the line endpoints to the visible area of the canvas. If we do
	   not do this, we have a line segment extending to COORD_MAX and our
	   math goes wrong.
	*/

	a.x = min (a.x, visible.x1);
	a.y = min (a.y, visible.y1);
	b.x = min (b.x, visible.x1);
	b.y = min (b.y, visible.y1);

	double d = distance_to_segment_squared (p, a, b, t, at);

	if (t < 0.0 || t > 1.0) {
		return false;
	}

	if (d < threshold) {
		return true;
	}

	return false;
}
