/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_LINESET_H__
#define __CANVAS_LINESET_H__

#include <vector>

#include "canvas/item.h"
#include "canvas/visibility.h"

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

	void add_coord (Coord, Distance, Gtkmm2ext::Color);
	void clear ();

	struct Line {
		Line (Coord p, Distance width_, Gtkmm2ext::Color color_) : pos (p), width (width_), color (color_) {}

		Coord pos;
		Distance width;
		Gtkmm2ext::Color color;
	};

private:
	std::vector<Line> _lines;
	Distance          _extent;
	Orientation       _orientation;
};

}

#endif /* __CANVAS_LINESET_H__ */
