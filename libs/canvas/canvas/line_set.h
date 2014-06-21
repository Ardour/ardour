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

#ifndef __CANVAS_LINESET_H__
#define __CANVAS_LINESET_H__

#include <vector>

#include "canvas/visibility.h"
#include "canvas/item.h"

namespace ArdourCanvas {

class LIBCANVAS_API LineSet : public Item
{
public:
	enum Orientation {
		Vertical,
		Horizontal
	};

	LineSet (Canvas*, Orientation o = Vertical);
	LineSet (Item*, Orientation o = Vertical);

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

        bool covers (Duple const &) const;

	void set_extent (Distance);
	Distance extent() const { return _extent; }

	void add (Coord, Distance, Color);
	void clear ();

	struct Line {
		Line (Coord p, Distance width_, Color color_) : pos (p), width (width_), color (color_) {}
		
		Coord pos;
		Distance width;
		Color color;
	};

private:
	std::vector<Line> _lines;
	Distance          _extent;
	Orientation       _orientation;
};

}

#endif /* __CANVAS_LINESET_H__ */
