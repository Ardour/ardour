/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/poly_item.h"

using namespace std;
using namespace ArdourCanvas;

PolyItem::PolyItem (Canvas* c)
	: Item (c)
{
}

PolyItem::PolyItem (Item* parent)
	: Item (parent)
{
}

void
PolyItem::compute_bounding_box () const
{
	if (!_points.empty ()) {
		Rect bbox;
		Points::const_iterator i = _points.begin ();

		bbox.x0 = bbox.x1 = i->x;
		bbox.y0 = bbox.y1 = i->y;

		++i;

		while (i != _points.end ()) {
			bbox.x0 = min (bbox.x0, i->x);
			bbox.y0 = min (bbox.y0, i->y);
			bbox.x1 = max (bbox.x1, i->x);
			bbox.y1 = max (bbox.y1, i->y);
			++i;
		}

		_bounding_box = bbox.expand (_outline_width + 0.5);

	} else {
		_bounding_box = Rect ();
	}

	bb_clean ();
}

void
PolyItem::render_path (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_points.size () < 2) {
		return;
	}

	const double pixel_adjust = (_outline_width == 1.0 ? 0.5 : 0.0);

	Points::const_iterator i  = _points.begin ();
	Duple                  c0 = item_to_window (Duple (i->x, i->y));

	++i;

	while (c0.x < -1.) {
		Duple c1 (item_to_window (Duple (i->x, i->y)));
		if (interpolate_line (c0, c1, -1)) {
			break;
		}
		if (++i == _points.end ()) {
			c1.x = 0;
			context->move_to (c1.x + pixel_adjust, c1.y + pixel_adjust);
			_left = _right = c1;
			return;
		}
		c0 = c1;
	}

	context->move_to (c0.x + pixel_adjust, c0.y + pixel_adjust);
	_left = c0;

	while (i != _points.end ()) {
		Duple c = item_to_window (Duple (i->x, i->y));
		if (c.x > area.x1) {
			if (interpolate_line (c0, c, area.x1)) {
				context->line_to (c0.x + pixel_adjust, c0.y + pixel_adjust);
			}
			break;
		}
		context->line_to (c.x + pixel_adjust, c.y + pixel_adjust);
		c0 = c;
		++i;
	}
	_right = c0;
}

bool
PolyItem::interpolate_line (Duple& c0, Duple const& c1, Coord const x)
{
	if (c1.x <= c0.x) {
		return false;
	}
	if (x < c0.x || x > c1.x) {
		return false;
	}

	c0.y += ((x - c0.x) / (c1.x - c0.x)) * (c1.y - c0.y);
	c0.x = x;
	return true;
}

void
PolyItem::set (Points const& points)
{
	if (_points != points) {
		begin_change ();

		_points = points;

		_bounding_box_dirty = true;
		end_change ();
	}
}

Points const&
PolyItem::get () const
{
	return _points;
}

void
PolyItem::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent () << '\t' << _points.size () << " points" << endl;
	for (Points::const_iterator i = _points.begin (); i != _points.end (); ++i) {
		o << _canvas->indent () << "\t\t" << i->x << ", " << i->y << endl;
	}
}
