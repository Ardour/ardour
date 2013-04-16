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
#include <boost/optional.hpp>

namespace ArdourCanvas
{

typedef double Coord;
typedef double Distance;
typedef uint32_t Color;

extern Coord const COORD_MAX;
extern Coord const CAIRO_MAX;

struct Duple
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

	Duple translate (Duple) const;
};


extern Duple operator- (Duple const &);
extern Duple operator+ (Duple const &, Duple const &);
extern Duple operator- (Duple const &, Duple const &);
extern Duple operator/ (Duple const &, double);
extern std::ostream & operator<< (std::ostream &, Duple const &);

struct Rect
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

	boost::optional<Rect> intersection (Rect const &) const;
	Rect extend (Rect const &) const;
	Rect translate (Duple) const;
	Rect expand (Distance) const;
	bool contains (Duple) const;
	Rect fix () const;

	Distance width () const {
		return x1 - x0;
	}

	Distance height () const {
		return y1 - y0;
	}
};

extern std::ostream & operator<< (std::ostream &, Rect const &);

typedef std::vector<Duple> Points;

}
	
#endif
