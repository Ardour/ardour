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
	, _radius (8)
	, _length (0)
	, bounding_parent (0)
{
}

Lollipop::Lollipop (Item* parent)
	: Item (parent)
	, _radius (8)
	, _length (0)
	, bounding_parent (0)
{
}

void
Lollipop::set_bounding_parent (Item* bp)
{
	bounding_parent = bp;
}

void
Lollipop::compute_bounding_box () const
{
	_bounding_box = Rect (_center.x -_radius, _center.y -_radius, _center.x + _radius, _center.y + _length + _radius);
	_bounding_box = _bounding_box.expand (2. * _outline_width);
	set_bbox_clean ();
}

void
Lollipop::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	setup_outline_context (context);

	Duple p = _parent->item_to_window (Duple (_center.x, _center.y));
	Duple l (p);

	if (fmod (_outline_width, 2.0)) {
		const Duple half_a_pixel (0.5 * _outline_width, 0.);
		l = l.translate (half_a_pixel);
	}

	/* the line */

	context->move_to (l.x, l.y + _radius);
	context->line_to (l.x, l.y + _length);
	context->stroke ();

	/* the circle: clip to avoid weirdness at top and bottom of parent */

	if (bounding_parent) {
		context->save ();
		Rect b (bounding_parent->item_to_window (bounding_parent->bounding_box()));
		context->rectangle (b.x0, b.y0, b.width(), b.height());
		context->clip();
	}

	context->arc (p.x, p.y, _radius, 0.0 * (M_PI/180.0), 360.0 * (M_PI/180.0));

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

	if (bounding_parent) {
		context->restore ();
	}

	render_children (area, context);
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
	if (x != _center.x) {
		begin_change ();
		_center.x = x;
		set_bbox_dirty ();
		end_change ();
	}
}

void
Lollipop::set_length (Coord len)
{
	if (_length != len) {
		begin_change ();
		_length = len;
		set_bbox_dirty ();
		end_change ();
	}
}

void
Lollipop::set (Duple const & d, Coord l, Coord r)
{
	begin_change ();

	_radius = r;
	_length = l;
	_center = d;

	set_bbox_dirty ();
	end_change ();
}

bool
Lollipop::covers (Duple const & point) const
{
	const Duple p = _parent->window_to_item (point);
	static const Distance threshold = 2.0;

	/* only the circle is considered as "covering" */

	if (((fabs (_center.x - p.x)) <= (_radius + threshold)) &&
	    ((fabs (_center.y - p.y)) <= (_radius + threshold))) {
		/* inside circle */
		return true;
	}

	return false;
}
