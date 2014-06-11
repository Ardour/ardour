/*
    Copyright (C) 2014 Paul Davis
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
#include "canvas/ruler.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Ruler::Ruler (Group *p, const Metric& m)
	: Item (p)
	, Fill (p)
	, Outline (p)
	, _metric (m)
{
}

void
Ruler::set_range (double l, double u)
{
	_lower = l;
	_upper = u;
}

void
Ruler::set_size (Rect const & area)
{
	if (_rect != area) {
		begin_visual_change ();
		_rect = area;
		_bounding_box_dirty = true;
		end_visual_change ();
	}
}

void
Ruler::compute_bounding_box () const
{
	if (!_rect.empty()) {
		_bounding_box = _rect;
	}

	_bounding_box_dirty = false;
}

void
Ruler::render (Rect const & area, Cairo::RefPtr<Cairo::Context> cr) const
{
	Rect self (item_to_window (_rect));
	boost::optional<Rect> i = self.intersection (area);
	if (!i) {
		return;
	}

	Rect intersection (i.get());

	vector<Mark> marks;
	Distance height = self.height();

	_metric.get_marks (marks, _lower, _upper, 50);

	/* draw background */

	setup_fill_context (cr);
	cr->rectangle (intersection.x0, intersection.y0, intersection.width(), intersection.height());
	cr->fill ();

	/* draw ticks */
	
	setup_outline_context (cr);

	for (vector<Mark>::const_iterator m = marks.begin(); m != marks.end(); ++m) {
		Duple pos;

		pos.x = self.x0 + ((m->position - _lower) / _metric.units_per_pixel);
		pos.y = self.y1; /* bottom edge */
		
		cr->move_to (pos.x, pos.y);
		
		switch (m->style) {
		case Mark::Major:
			cr->rel_move_to (0, -height);
			break;
		case Mark::Minor:
			cr->rel_move_to (0, -(height/2.0));
			break;
		case Mark::Micro:
			cr->rel_move_to (0, pos.y-(height/4.0));
			break;
		}
		cr->stroke ();
	}

	/* done! */
}
