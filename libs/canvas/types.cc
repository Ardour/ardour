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
	static const Coord BIG = COORD_MAX/10.0; /* 1 order of magnitude from COORD_MAX */

	const Distance w = r.width();
	const Distance h = r.height();

	s << "[(";

	if (r.x0 > BIG) {
		s << "BIG";
	} else {
		s << r.x0;
	}

	s << ", ";

	if (r.y0 > BIG) {
		s << "BIG";
	} else {
		s << r.y0;
	}

	s << "), (";


	if (r.x1 > BIG) {
		s << "BIG";
	} else {
		s << r.x1;
	}

	s << ", ";

	if (r.y1 > BIG) {
		s << "BIG";
	} else {
		s << r.y1;
	}

	s << ") ";

	if (w > BIG) {
		s << "BIG";
	} else {
		s << w;
	}

	s << " x ";

	if (h > BIG) {
		s << "BIG";
	} else {
		s << h;
	}

	s << ']';
	return s;
}
