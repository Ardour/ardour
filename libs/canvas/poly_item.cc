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

#include "pbd/compose.h"

#include "canvas/poly_item.h"
#include "canvas/canvas.h"

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

	if (!_points.empty()) {

		Rect bbox;
		Points::const_iterator i = _points.begin();

		bbox.x0 = bbox.x1 = i->x;
		bbox.y0 = bbox.y1 = i->y;

		++i;

		while (i != _points.end()) {
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

	_bounding_box_dirty = false;
}

void
PolyItem::render_path (Rect const & /* area */, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_points.size() < 2) {
		return;
	}

	Points::const_iterator i = _points.begin();
	Duple c (item_to_window (Duple (i->x, i->y)));
	const double pixel_adjust = (_outline_width == 1.0 ? 0.5 : 0.0);

	context->move_to (c.x + pixel_adjust, c.y + pixel_adjust);
	++i;

	while (i != _points.end()) {
		c = item_to_window (Duple (i->x, i->y));
		context->line_to (c.x + pixel_adjust, c.y + pixel_adjust);
		++i;
	}
}

void
PolyItem::render_curve (Rect const & area, Cairo::RefPtr<Cairo::Context> context, Points const & first_control_points, Points const & second_control_points) const
{
	if (_points.size() <= 2) {
		render_path (area, context);
		return;
	}

	Points::const_iterator cp1 = first_control_points.begin();
	Points::const_iterator cp2 = second_control_points.begin();
	Points::const_iterator p = _points.begin();
	const double pixel_adjust = (_outline_width == 1.0 ? 0.5 : 0.0);

	Duple c = item_to_window (Duple (p->x, p->y));
	context->move_to (c.x + pixel_adjust, c.y + pixel_adjust);
	++p;

	while (p != _points.end()) {

		Duple c1 = item_to_window (Duple (cp1->x, cp1->y));
		Duple c2 = item_to_window (Duple (cp2->x, cp2->y));

		c = item_to_window (Duple (p->x, p->y));

		context->curve_to (c1.x + pixel_adjust,
				   c1.y + pixel_adjust,
				   c2.x + pixel_adjust,
				   c2.y + pixel_adjust,
				   c.x + pixel_adjust,
				   c.y + pixel_adjust);

		++cp1;
		++cp2;
		++p;
	}
}

void
PolyItem::set (Points const & points)
{
	if (_points != points) {

		begin_change ();

		_points = points;

		_bounding_box_dirty = true;
		end_change ();
	}
}

Points const &
PolyItem::get () const
{
	return _points;
}

void
PolyItem::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent() << '\t' << _points.size() << " points" << endl;
	for (Points::const_iterator i = _points.begin(); i != _points.end(); ++i) {
		o << _canvas->indent() << "\t\t" << i->x << ", " << i->y << endl;
	}
}
