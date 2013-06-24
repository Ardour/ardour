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

#include <algorithm>
#include <cfloat>
#include <cassert>

#include <cairomm/context.h>

#include "canvas/types.h"

using namespace std;
using namespace ArdourCanvas;

Coord const ArdourCanvas::COORD_MAX = 1.7e307;

static inline Coord
safe_add (Coord a, Coord b)
{
	if (((COORD_MAX - a) <= b) || ((COORD_MAX - b) <= a)) {
		return COORD_MAX;
	}

	return a + b;
}

Duple
Duple::translate (Duple t) const
{
	Duple d;

	d.x = safe_add (x, t.x);
	d.y = safe_add (y, t.y);
	
	return d;
}

boost::optional<Rect>
Rect::intersection (Rect const & o) const
{
	Rect i;
	
	i.x0 = max (x0, o.x0);
	i.y0 = max (y0, o.y0);
	i.x1 = min (x1, o.x1);
	i.y1 = min (y1, o.y1);

	if (i.x0 > i.x1 || i.y0 > i.y1) {
		return boost::optional<Rect> ();
	}
	
	return boost::optional<Rect> (i);
}

Rect
Rect::translate (Duple t) const
{
	Rect r;

	r.x0 = safe_add (x0, t.x);
	r.y0 = safe_add (y0, t.y);
	r.x1 = safe_add (x1, t.x);
	r.y1 = safe_add (y1, t.y);
	return r;
}

Rect
Rect::extend (Rect const & o) const
{
	Rect r;
	r.x0 = min (x0, o.x0);
	r.y0 = min (y0, o.y0);
	r.x1 = max (x1, o.x1);
	r.y1 = max (y1, o.y1);
	return r;
}

Rect
Rect::expand (Distance amount) const
{
	Rect r;
	r.x0 = x0 - amount;
	r.y0 = y0 - amount;
	r.x1 = safe_add (x1, amount);
	r.y1 = safe_add (y1, amount);
	return r;
}

bool
Rect::contains (Duple point) const
{
	return point.x >= x0 && point.x <= x1 && point.y >= y0 && point.y <= y1;
}

Rect
Rect::fix () const
{
	Rect r;
	
	r.x0 = min (x0, x1);
	r.y0 = min (y0, y1);
	r.x1 = max (x0, x1);
	r.y1 = max (y0, y1);

	return r;
}

Rect
Rect::convert_to_device (Cairo::RefPtr<Cairo::Context> c) const
{
	Coord xa, ya, xb, yb;

	xa = x0;
	xb = x1;
	ya = y0;
	yb = y1;

	c->user_to_device (xa, ya);
	c->user_to_device (xb, yb);

	return Rect (xa, ya, xb, yb);
}


Rect
Rect::convert_to_user (Cairo::RefPtr<Cairo::Context> c) const
{
	Coord xa, ya, xb, yb;

	xa = x0;
	xb = x1;
	ya = y0;
	yb = y1;

	c->device_to_user (xa, ya);
	c->device_to_user (xb, yb);

	return Rect (xa, ya, xb, yb);
}

Duple
ArdourCanvas::operator- (Duple const & o)
{
	return Duple (-o.x, -o.y);
}

Duple
ArdourCanvas::operator+ (Duple const & a, Duple const & b)
{
	return Duple (safe_add (a.x, b.x), safe_add (a.y, b.y));
}

bool
ArdourCanvas::operator== (Duple const & a, Duple const & b)
{
	return a.x == b.x && a.y == b.y;
}

Duple
ArdourCanvas::operator- (Duple const & a, Duple const & b)
{
	return Duple (a.x - b.x, a.y - b.y);
}

Duple
ArdourCanvas::operator/ (Duple const & a, double b)
{
	return Duple (a.x / b, a.y / b);
}

ostream &
ArdourCanvas::operator<< (ostream & s, Duple const & r)
{
	s << "(" << r.x << ", " << r.y << ")";
	return s;
}

ostream &
ArdourCanvas::operator<< (ostream & s, Rect const & r)
{
	s << "[(" << r.x0 << ", " << r.y0 << "), (" << r.x1 << ", " << r.y1 << ") " << r.width() << " x " << r.height() << "]";
	return s;
}

