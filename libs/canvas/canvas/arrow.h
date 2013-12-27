/*
    Copyright (C) 2011 Paul Davis
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

/** @file  canvas/arrow.h
 *  @brief Declaration of the Arrow canvas object.
 */

#ifndef __CANVAS_ARROW_H__
#define __CANVAS_ARROW_H__

#include "canvas/visibility.h"

#include "canvas/group.h"

namespace ArdourCanvas {

class Line;
class Polygon;

/** A composite item which draws a line with arrow heads
 *  at either or both ends.
 *
 *  The arrow heads are identified by the indices 0 and 1;
 *  head 0 is at the (x0, y0) end of the line, and head 1
 *  at the (x1, y1) end.
 *
 *  @todo Draws vertical lines only; could be generalised
 *  to draw lines at any angle.
 */
	
class LIBCANVAS_API Arrow : public Group
{
public:
	Arrow (Group *);

	void set_show_head (int, bool);
	void set_head_outward (int, bool);
	void set_head_height (int, Distance);
	void set_head_width (int, Distance);
	void set_outline_width (Distance);
	void set_color (Color);

	Coord x () const;
	Coord y1 () const;

	void set_x (Coord);
	void set_y0 (Coord);
	void set_y1 (Coord);
	
        bool covers (Duple const &) const;

private:
	void setup_polygon (int);

	/** Representation of a single arrow head */
	struct Head {
		Polygon* polygon; ///< the polygon which represents its shape
		bool show;        ///< true if this head should be visible
		bool outward;     ///< true if this head points out from the line
		Distance height;  ///< the height of the head
		Distance width;   ///< the maximum width of the head
	};

	/** our arrow heads; _heads[0] is at the (x0, y0) end of the line,
	 *  and _heads[1] at the (x1, y1) end.
	 */
	Head _heads[2];

	/** our line */
	Line* _line;
};

}

#endif
