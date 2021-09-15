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

#ifndef __CANVAS_TYPES_H__
#define __CANVAS_TYPES_H__

#include <iostream>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <boost/optional.hpp>

#include <cairomm/refptr.h>

#include "gtkmm2ext/colors.h"

#include "canvas/visibility.h"

namespace Cairo {
	class Context;
}

namespace ArdourCanvas
{

typedef double Coord;
typedef double Distance;

extern LIBCANVAS_API Coord const COORD_MAX;

enum Orientation {
	Horizontal = 0x1,
	Vertical = 0x2,
};

inline Coord
canvas_safe_add (Coord a, Coord b)
{
	if (((COORD_MAX - a) <= b) || ((COORD_MAX - b) <= a)) {
		return COORD_MAX;
	}

	return a + b;
}


struct LIBCANVAS_API Duple
{
	Duple ()
		: x (0)
		, y (0)
	{}

	Duple (Coord x_, Coord y_)
		: x (x_)
		, y (y_)
	{}

	Coord x;
	Coord y;

	/* alias */

	Coord width() const { return x; }
	Coord height() const { return y; }

	Duple translate (const Duple& t) const throw() {
		return Duple (canvas_safe_add (x, t.x), canvas_safe_add (y, t.y));
	}

	Duple operator- () const throw () {
		return Duple (-x, -y);
	}
	Duple operator+ (Duple const & o) const throw () {
		return Duple (canvas_safe_add (x, o.x), canvas_safe_add (y, o.y));
	}
	bool operator== (Duple const & o) const throw () {
		return x == o.x && y == o.y;
	}
	bool operator!= (Duple const & o) const throw () {
		return x != o.x || y != o.y;
	}
	Duple operator- (Duple const & o) const throw () {
		return Duple (x - o.x, y - o.y);
	}
	Duple operator/ (double b) const throw () {
		return Duple (x / b, y / b);
	}
};


extern LIBCANVAS_API std::ostream & operator<< (std::ostream &, Duple const &);

struct LIBCANVAS_API Rect
{
	Rect ()
		: x0 (0)
		, y0 (0)
		, x1 (0)
		, y1 (0)
	{}

	Rect (Coord x0_, Coord y0_, Coord x1_, Coord y1_)
		: x0 (x0_)
		, y0 (y0_)
		, x1 (x1_)
		, y1 (y1_)
	{}

	Coord x0;
	Coord y0;
	Coord x1;
	Coord y1;

	Rect intersection (Rect const & o) const throw () {
		Rect i (std::max (x0, o.x0), std::max (y0, o.y0),
			std::min (x1, o.x1), std::min (y1, o.y1));

		if (i.x0 > i.x1 || i.y0 > i.y1) {
			return Rect();
		}

		return i;
	}

	Rect extend (Rect const & o) const throw () {
		return Rect (std::min (x0, o.x0), std::min (y0, o.y0),
			     std::max (x1, o.x1), std::max (y1, o.y1));
	}
	Rect translate (Duple const& t) const throw () {
		return Rect (canvas_safe_add (x0, t.x), canvas_safe_add (y0, t.y),
			     canvas_safe_add (x1, t.x),canvas_safe_add (y1, t.y));
	}
	Rect expand (Distance amount) const throw () {
		return Rect (x0 - amount, y0 - amount,
			     canvas_safe_add (x1, amount),
			     canvas_safe_add (y1, amount));
	}
	Rect expand (Distance top, Distance right, Distance bottom, Distance left) const throw () {
		return Rect (x0 - left, y0 - top,
			     canvas_safe_add (x1, right),
			     canvas_safe_add (y1, bottom));
	}

	Rect shrink (Distance amount) const throw () {
		/* This isn't the equivalent of expand (-distance) because
		   of the peculiarities of canvas_safe_add() with negative values.
		   Maybe.
		*/
		return Rect (canvas_safe_add (x0, amount), canvas_safe_add (y0, amount),
			     x1 - amount, y1 - amount);
	}

	Rect shrink (Distance top, Distance right, Distance bottom, Distance left) const throw () {
		/* This isn't the equivalent of expand (-distance) because
		   of the peculiarities of canvas_safe_add() with negative values.
		   Maybe.
		*/
		return Rect (canvas_safe_add (x0, left), canvas_safe_add (y0, top),
			     x1 - right, y1 - bottom);
	}

	bool contains (Duple const & point) const throw () {
		return point.x >= x0 && point.x < x1 && point.y >= y0 && point.y < y1;
	}
	Rect fix () const throw () {
		return Rect (std::min (x0, x1), std::min (y0, y1),
			     std::max (x0, x1), std::max (y0, y1));
	}

	bool empty() const throw () { return (x0 == x1 && y0 == y1); }
	operator bool() const throw () { return !empty(); }

	Distance width () const  throw () {
		return x1 - x0;
	}

	Distance height () const throw () {
		return y1 - y0;
	}
	bool operator!= (Rect const & o) const throw () {
		return x0 != o.x0 ||
			x1 != o.x1 ||
			y0 != o.y0 ||
			y1 != o.y1;
	}
};

enum PackOptions {
	PackExpand = 0x1, /* use all available space ... */
	PackFill = 0x2,   /* if PackExpand set, this means actually
	                     expand size of Item; if PackExpand not
	                     set, this does nothing.
	                  */
	PackShrink = 0x4, /* allow Item to be smaller than its natural size */
	PackFromStart = 0x8,
	PackFromEnd = 0x10
};

struct FourDimensions {
	Distance up;
	Distance right;
	Distance down;
	Distance left;

	FourDimensions (Distance u, Distance r = -1., Distance d = -1., Distance l = -1.) {

		/* CSS style defaults: see https://developer.mozilla.org/en-US/docs/Web/CSS/Shorthand_properties
		 */

		Distance args[4];
		uint32_t nargs = 1;

		args[0] = u;

		if (r >= 0) { args[1] = r; ++nargs; }
		if (d >= 0) { args[2] = d; ++nargs; }
		if (l >= 0) { args[3] = l; ++nargs; }

		switch (nargs) {
		case 1:
			up = right = down = left = args[0];
			break;
		case 2:
			up = down = args[0];
			left = right = args[1];
			break;
		case 3:
			up = args[0];
			left = right = args[1];
			down = args[2];
			break;
		case 4:
			up = args[0];
			right = args[1];
			down = args[2];
			left = args[3];
			break;
		}
	}
};

extern LIBCANVAS_API std::ostream & operator<< (std::ostream &, Rect const &);

typedef std::vector<Duple> Points;

}

#endif
