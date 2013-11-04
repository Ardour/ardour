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

#include "canvas/line_set.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

/* XXX: hard-wired to horizontal only */

class LineSorter {
public:
	bool operator() (LineSet::Line& a, LineSet::Line& b) {
		return a.y < b.y;
	}
};

LineSet::LineSet (Group* parent)
	: Item (parent)
	, _height (0)
{

}


void
LineSet::compute_bounding_box () const
{
	if (_lines.empty ()) {
		_bounding_box = boost::optional<Rect> ();
	} else {
		_bounding_box = Rect (0, _lines.front().y - (_lines.front().width/2.0), COORD_MAX, min (_height, _lines.back().y - (_lines.back().width/2.0)));
	}
	_bounding_box_dirty = false;
}

void
LineSet::set_height (Distance height)
{
	begin_change ();

	_height = height;

	_bounding_box_dirty = true;
	end_change ();
}

void
LineSet::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* area is in window coordinates */

	for (list<Line>::const_iterator i = _lines.begin(); i != _lines.end(); ++i) {

		Rect self = item_to_window (Rect (0, i->y - (i->width/2.0), COORD_MAX, i->y + (i->width/2.0)));
		boost::optional<Rect> intersect = self.intersection (area);
			
		if (!intersect) {
			continue;	
		}

		set_source_rgba (context, i->color);
		context->set_line_width (i->width);
		context->move_to (intersect->x0, self.y0 + ((self.y1 - self.y0)/2.0));
		context->line_to (intersect->x1, self.y0 + ((self.y1 - self.y0)/2.0));
		context->stroke ();
	}
}

void
LineSet::add (Coord y, Distance width, Color color)
{
	begin_change ();
	
	_lines.push_back (Line (y, width, color));
	_lines.sort (LineSorter ());

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
	return false;
}
