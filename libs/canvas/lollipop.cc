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
#include "canvas/lollipop.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Lollipop::Lollipop (Canvas* c)
	: Item (c)
{
}

Lollipop::Lollipop (Item* parent)
	: Item (parent)
{
}

void
Lollipop::compute_bounding_box () const
{
	Rect bbox;

	bbox.x0 = min (_points[0].x, _points[1].x);
	bbox.y0 = min (_points[0].y, _points[1].y);
	bbox.x1 = max (_points[0].x, _points[1].x) + _radius;
	bbox.y1 = max (_points[0].y, _points[1].y) + _radius;

	bbox = bbox.expand (0.5 + (_outline_width / 2));

	_bounding_box = bbox;
	set_bbox_clean ();
}

void
Lollipop::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
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

	/* the circle */

	context->arc (p1.x, p1.y, _radius, 0.0 * (M_PI/180.0), 360.0 * (M_PI/180.0));

	if (fill()) {
		setup_fill_context (context);
		if (outline()) {
			context->fill_preserve ();
		} else {
			context->fill ();
		}
	}

	if (outline()) {
		setup_outline_context (context);
		context->stroke ();
	}
}

void
Lollipop::set_radius (Coord r)
{
	if (_radius != r) {
		begin_change ();
		_radius = r;
		set_bbox_dirty ();
		end_change ();
	}
}

void
Lollipop::set_x (Coord x)
{
	if (x != _points[0].x) {
		begin_change ();

		_points[0].x = x;
		_points[1].x = x;

		set_bbox_dirty ();
		end_change ();
	}
}

void
Lollipop::set_length (Coord len)
{
	if (_points[1].y != _points[0].y - len) {
		begin_change ();
		_points[1].y = _points[0].y - len;
		end_change ();
	}
}

void
Lollipop::set (Duple const & d, Coord l, Coord r)
{
	begin_change ();

	_points[0].x = d.x;
	_points[1].x = d.x;

	_points[0].y = d.y;
	_points[1].y = _points[0].y - l;

	_radius = r;

	end_change ();
}

bool
Lollipop::covers (Duple const & point) const
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
