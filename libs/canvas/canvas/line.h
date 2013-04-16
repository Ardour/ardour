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

#ifndef __CANVAS_LINE_H__
#define __CANVAS_LINE_H__

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/poly_line.h"

namespace ArdourCanvas {

class Line : virtual public Item, public Outline
{
public:
	Line (Group *);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set (Duple, Duple);
	void set_x0 (Coord);
	void set_y0 (Coord);
	void set_x1 (Coord);
	void set_y1 (Coord);
	Coord x0 () const {
		return _points[0].x;
	}
	Coord y0 () const {
		return _points[0].y;
	}
	Coord x1 () const {
		return _points[1].x;
	}
	Coord y1 () const {
		return _points[1].y;
	}

private:
	Duple _points[2];
};
	
}

#endif
