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

#include <cfloat>
#include <cassert>

#include <cairomm/context.h>

#include "canvas/types.h"

using namespace std;
using namespace ArdourCanvas;

Coord const ArdourCanvas::COORD_MAX = 1.7e307;

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

