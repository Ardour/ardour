/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_LINE_H__
#define __CANVAS_LINE_H__

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/poly_line.h"
#include "canvas/visibility.h"

namespace ArdourCanvas {

class LIBCANVAS_API Line : public Item
{
public:
	Line (Canvas*);
	Line (Item*);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
	bool covers (Duple const &) const;

	void set (Duple, Duple);
	void set_x0 (Coord);
	void set_y0 (Coord);
	void set_x1 (Coord);
	void set_y1 (Coord);
	void set_x (Coord, Coord);

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
