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

#include <cmath>

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

			double y0 = _lines.front().pos - (_lines.front().width/2.0);
			double y1 = _lines.back().pos + (_lines.back().width/2.0);

			if (fmod (_lines.front().width, 2.)) {
				y0 -= _lines.front().width * 0.5;
			}

			_bounding_box = Rect (0, y0, _extent, y1);

		} else {

			double x0 = _lines.front().pos - _lines.front().width/2.0;
			double x1 = _lines.back().pos + _lines.back().width/2.0;

			if (fmod (_lines.front().width, 2.)) {
				x0 -= _lines.front().width * 0.5;
			}

			_bounding_box = Rect (x0, 0, x1, _extent);
		}
	}

	set_bbox_clean ();
}

void
LineSet::set_extent (Distance e)
{
	begin_change ();

	_extent = e;
	set_bbox_dirty ();

	end_change ();
}

void
LineSet::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* area is in window coordinates */

	for (auto const & l : _lines) {

		Rect self;

		/* Self is a rectangle that fully encloses the pixels drawn to
		 * display this line. It does NOT represent the coordinates
		 * that should be used to actually draw the line, at least not
		 * along the axis corresponding to _orientation
		 */

		if (_orientation == Horizontal) {
			self = Rect (0, l.pos - (l.width/2.0), _extent, l.pos + (l.width/2.0));
		} else {
			self = Rect (l.pos - (l.width/2.0), 0, l.pos + (l.width/2.0), _extent);
		}

		/* Note the 2nd argument, to avoid rounding after we may have
		 * just shifted to a half-pixel grid (see
		 * doc/cairo-single-pixel-lines)
		 */
		self = item_to_window (self, false);
		Rect intersection = self.intersection (area);

		if (!intersection) {
			continue;
		}

		Gtkmm2ext::set_source_rgba (context, l.color);
		context->set_line_width (l.width);

		/* OK, something to draw. The actual line must be drawn at
		 * l.pos (one one axis)
		 */

		if (_orientation == Horizontal) {
			Coord c = item_to_window (Duple (0., l.pos), false).y;
			context->move_to (intersection.x0, c);
			context->line_to (intersection.x1, c);
		} else {
			Coord c = item_to_window (Duple (l.pos, 0.), false).x;
			context->move_to (c, intersection.y0);
			context->line_to (c, intersection.y1);
		}

		context->stroke ();
	}
}

void
LineSet::add_coord (Coord pos, Distance width, Gtkmm2ext::Color color)
{
	/* If width is odd (width % 2 != 0) and position is on
	   a whole pixel, shift it to a half-pixel position. Otherwise
	   force it back to an integer position. See
	   doc/cairo-single-pixel-lines for more details.
	*/

	if (fmod (width, 2.) && !fmod (pos, 1.0)) {
		/* odd width, integral position */
		pos += 0.5;
	} else {
		/* even width and/or non-integral position */
		pos = floor (pos);
	}

	_lines.push_back (Line (pos, width, color));
}

void
LineSet::begin_add ()
{
	begin_change ();
}

void
LineSet::end_add ()
{
	if (!_lines.empty()) {
		sort (_lines.begin(), _lines.end(), LineSorter());
	}

	set_bbox_dirty ();
	end_change ();
}

void
LineSet::clear ()
{
	begin_change ();
	_lines.clear ();
	set_bbox_dirty ();
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
