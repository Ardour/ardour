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

#ifndef __CANVAS_FILL_H__
#define __CANVAS_FILL_H__

#include <vector>
#include <stdint.h>

#include "canvas/visibility.h"
#include "canvas/item.h"

namespace ArdourCanvas {

class LIBCANVAS_API Fill : virtual public Item
{
public:
	Fill (Group *);

	virtual void set_fill_color (Color);
	virtual void set_fill (bool);

	Color fill_color () const {
		return _fill_color;
	}

	bool fill () const {
		return _fill;
	}

        typedef std::vector<std::pair<double,Color> > StopList;
	
        void set_gradient (StopList const & stops, bool is_vertical);

protected:
	void setup_fill_context (Cairo::RefPtr<Cairo::Context>) const;
        void setup_gradient_context (Cairo::RefPtr<Cairo::Context>, Rect const &, Duple const &) const;
       	
	Color _fill_color;
	bool _fill;
        StopList _stops;
        bool _vertical_gradient;
};

}

#endif
