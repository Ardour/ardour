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

#ifndef __CANVAS_RECTANGLE_H__
#define __CANVAS_RECTANGLE_H__

#include "canvas/visibility.h"
#include "canvas/item.h"
#include "canvas/types.h"
#include "canvas/outline.h"
#include "canvas/fill.h"

namespace ArdourCanvas
{

class LIBCANVAS_API Rectangle : virtual public Item, public Outline, public Fill
{
public:
	Rectangle (Group *);
	Rectangle (Group *, Rect const &);
	
	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	Rect const & get () const {
		return _rect;
	}

	Coord x0 () const {
		return _rect.x0;
	}

	Coord y0 () const {
		return _rect.y0;
	}

	Coord x1 () const {
		return _rect.x1;
	}

	Coord y1 () const {
		return _rect.y1;
	}

	void set (Rect const &);
	void set_x0 (Coord);
	void set_y0 (Coord);
	void set_x1 (Coord);
	void set_y1 (Coord);

	enum What {
		LEFT = 0x1,
		RIGHT = 0x2,
		TOP = 0x4,
		BOTTOM = 0x8
	};

	void set_outline_what (What);
	void set_outline_what (int);

private:
	/** Our rectangle; note that x0 may not always be less than x1
	 *  and likewise with y0 and y1.
	 */
	Rect _rect;
	What _outline_what;
};

}

#endif
