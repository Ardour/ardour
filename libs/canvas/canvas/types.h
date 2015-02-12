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

#ifndef __CANVAS_TYPES_H__
#define __CANVAS_TYPES_H__

#include <iostream>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <boost/optional.hpp>

#include <cairomm/refptr.h>

#include "canvas/visibility.h"

namespace Cairo {
 	class Context;
}

namespace ArdourCanvas
{

typedef double Coord;
typedef double Distance;
typedef uint32_t Color;

extern LIBCANVAS_API Coord const COORD_MAX;

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

	boost::optional<Rect> intersection (Rect const & o) const throw () {
		Rect i (std::max (x0, o.x0), std::max (y0, o.y0),
			std::min (x1, o.x1), std::min (y1, o.y1));
		
		if (i.x0 > i.x1 || i.y0 > i.y1) {
			return boost::optional<Rect> ();
		}
		
		return boost::optional<Rect> (i);
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

	Rect shrink (Distance amount) const throw () {
		/* This isn't the equivalent of expand (-distance) because
		   of the peculiarities of canvas_safe_add() with negative values.
		   Maybe.
		*/
		return Rect (canvas_safe_add (x0, amount), canvas_safe_add (y0, amount),
			     x1 - amount, y1 - amount);
	}
		
	bool contains (Duple const & point) const throw () {
		return point.x >= x0 && point.x < x1 && point.y >= y0 && point.y < y1;
	}
	Rect fix () const throw () {
		return Rect (std::min (x0, x1), std::min (y0, y1),
			     std::max (x0, x1), std::max (y0, y1));
	}
		
	bool empty() const throw () { return (x0 == x1 && y0 == y1); }

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

extern LIBCANVAS_API std::ostream & operator<< (std::ostream &, Rect const &);

typedef std::vector<Duple> Points;

}
	
#endif
