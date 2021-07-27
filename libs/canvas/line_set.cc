/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "canvas/line_set.h"

using namespace std;
using namespace ArdourCanvas;

class LineSorter {
public:
	bool operator() (LineSet::Line const & a, LineSet::Line const & b) {
		return a.pos < b.pos;
	}
};

LineSet::LineSet (Canvas* c, Orientation o)
	: Item (c)
	, _extent (0)
	, _orientation (o)
{

}

LineSet::LineSet (Item* parent, Orientation o)
	: Item (parent)
	, _extent (0)
	, _orientation (o)
{

}

void
LineSet::compute_bounding_box () const
{
	if (_lines.empty ()) {
		_bounding_box = Rect ();
	} else {

		if (_orientation == Horizontal) {

			_bounding_box = Rect (0, /* x0 */
					      _lines.front().pos - (_lines.front().width/2.0), /* y0 */
					      _extent, /* x1 */
					      _lines.back().pos - (_lines.back().width/2.0) /* y1 */
				);

		} else {

			_bounding_box = Rect (_lines.front().pos - _lines.front().width/2.0, /* x0 */
					      0, /* y0 */
					      _lines.back().pos + _lines.back().width/2.0, /* x1 */
					      _extent /* y1 */
				);
		}
	}

	bb_clean ();
}

void
LineSet::set_extent (Distance e)
{
	begin_change ();

	_extent = e;
	_bounding_box_dirty = true;

	end_change ();
}

void
LineSet::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* area is in window coordinates */

	for (vector<Line>::const_iterator i = _lines.begin(); i != _lines.end(); ++i) {

		Rect self;

		if (_orientation == Horizontal) {
			self = item_to_window (Rect (0, i->pos - (i->width/2.0), _extent, i->pos + (i->width/2.0)));
		} else {
			self = item_to_window (Rect (i->pos - (i->width/2.0), 0, i->pos + (i->width/2.0), _extent));
		}

		Rect isect = self.intersection (area);

		if (!isect) {
			continue;
		}

		Rect intersection (isect);

		Gtkmm2ext::set_source_rgba (context, i->color);
		context->set_line_width (i->width);

		/* Not 100% sure that the computation of the invariant
		 * positions (y and x) below work correctly if the line width
		 * is not 1.0, but visual inspection suggests it is OK.
		 */

		if (_orientation == Horizontal) {
			double y = self.y0 + ((self.y1 - self.y0)/2.0);
			context->move_to (intersection.x0, y);
			context->line_to (intersection.x1, y);
		} else {
			double x = self.x0 + ((self.x1 - self.x0)/2.0);
			context->move_to (x, intersection.y0);
			context->line_to (x, intersection.y1);
		}

		context->stroke ();
	}
}

void
LineSet::add_coord (Coord y, Distance width, Gtkmm2ext::Color color)
{
	begin_change ();

	_lines.push_back (Line (y, width, color));
	sort (_lines.begin(), _lines.end(), LineSorter());

	_bounding_box_dirty = true;
	end_change ();
}

void
LineSet::clear ()
{
	begin_change ();
	_lines.clear ();
	_bounding_box_dirty = true;
	end_change ();
}

bool
LineSet::covers (Duple const & /*point*/) const
{
	/* lines are ordered by position along primary axis, so binary search
	 * to find the place to start looking.
	 *
	 * XXX but not yet.
	 */

	return false;
}
