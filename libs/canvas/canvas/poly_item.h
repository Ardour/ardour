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

#ifndef __CANVAS_POLY_ITEM_H__
#define __CANVAS_POLY_ITEM_H__

#include "canvas/item.h"
#include "canvas/outline.h"

namespace ArdourCanvas {

class PolyItem : virtual public Item, public Outline
{
public:
	PolyItem (Group *);

	void compute_bounding_box () const;

	virtual void set (Points const &);
	Points const & get () const;

        void dump (std::ostream&) const;

protected:
	void render_path (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
        void render_curve (Rect const &, Cairo::RefPtr<Cairo::Context>, Points const &, Points const &) const;

	Points _points;
};
	
}

#endif
