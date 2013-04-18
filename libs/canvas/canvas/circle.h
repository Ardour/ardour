/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __CANVAS_CIRCLE_H__
#define __CANVAS_CIRCLE_H__

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/fill.h"

namespace ArdourCanvas {

class Circle : virtual public Item, public Outline, public Fill
{
public:
	Circle (Group *);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

        void set_center (Duple const &);
	void set_radius (Coord);

        Duple center() const { 
		return _center;
	}
	Coord radius () const {
		return _radius;
	}

private:
        Duple _center;
        Coord _radius;
};
	
}

#endif
