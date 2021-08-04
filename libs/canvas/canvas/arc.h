/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_ARC_H__
#define __CANVAS_ARC_H__

#include "canvas/fill.h"
#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/visibility.h"

namespace ArdourCanvas {

class Canvas;

class LIBCANVAS_API Arc : public Item
{
public:
	Arc (Canvas*);
	Arc (Item*);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void _size_allocate (Rect const & r);

	void set_center (Duple const &);
	void set_radius (Coord);
	void set_arc (double degrees);
	void set_start (double degrees);

	Duple center() const {
		return _center;
	}
	Coord radius () const {
		return _radius;
	}
	double arc_degrees () const {
		return _arc_degrees;
	}
	double start_degrees () const {
		return _start_degrees;
	}

	bool covers (Duple const &) const;

private:
	Duple  _center;
	Coord  _radius;
	double _arc_degrees;
	double _start_degrees;
};

}

#endif
