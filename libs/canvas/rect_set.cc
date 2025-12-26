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

#include "canvas/rect_set.h"

using namespace std;
using namespace ArdourCanvas;

RectSet::RectSet (Canvas* c)
	: Item (c)
{

}

RectSet::RectSet (Item* parent)
	: Item (parent)
{

}

void
RectSet::compute_bounding_box () const
{
	if (_rects.empty ()) {
		_bounding_box = Rect ();
	} else {
		Rect rb;

		for (auto const & r : _rects) {
			rb = rb.extend (r);
		}

		_bounding_box = rb;
	}

	set_bbox_clean ();
}

void
RectSet::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* area is in window coordinates */

	Rect p (parent()->item_to_window (parent()->bounding_box()).intersection (area));

	for (auto const & r : _rects) {

		Rect self;

		self = item_to_window (r, false);
		Rect intersection = self.intersection (p);

		if (!intersection) {
			continue;
		}

		Gtkmm2ext::set_source_rgba (context, r.color);

		/* XXXX another example of where using COORD_MAX fails
		 * (presumably at some point Cairo folds from double to an
		 * integral and overflows). Use a "big number" that isn't too
		 * big.
		 *
		 * We should really find out what the upper limit actually is
		 * and use that for COORD_MAX.
		 */

		context->rectangle (intersection.x0, intersection.y0, intersection.width(), intersection.height());
		context->fill ();
	}
}

void
RectSet::add_rect (int index, Rect const & ra, Gtkmm2ext::Color color)
{
	_rects.push_back (ColoredRectangle (index, ra, color));
}

void
RectSet::begin_add ()
{
	begin_change ();
}

void
RectSet::end_add ()
{
	set_bbox_dirty ();
	end_change ();
}

void
RectSet::clear ()
{
	begin_change ();
	_rects.clear ();
	set_bbox_dirty ();
	end_change ();
}

bool
RectSet::covers (Duple const & point) const
{
	if (bounding_box().contains (point)) {
		return true;
	}

	for (auto const & r : _rects) {
		if (r.contains (point)) {
			return true;
		}
	}

	return false;
}
