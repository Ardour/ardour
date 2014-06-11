/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __CANVAS_RULER_H__
#define __CANVAS_RULER_H__

#include <string>
#include <vector>

#include "canvas/item.h"
#include "canvas/fill.h"
#include "canvas/outline.h"

namespace ArdourCanvas
{
	
class LIBCANVAS_API Ruler : virtual public Item, public Fill, public Outline
{
public:
	struct Mark {
		enum Style {
			Major,
			Minor,
			Micro
		};
		std::string label;
		double position;
		Style  style;
	};
	
	struct Metric {
		Metric () : units_per_pixel (0) {}
		virtual ~Metric() {}

		double units_per_pixel;

		/* lower and upper and sample positions, which are also canvas coordinates
		 */
		
		virtual int get_marks (std::vector<Mark>&, double lower, double upper, int maxchars) const = 0;
	};
	
	Ruler (Group *, const Metric& m);
	
	void set_range (double lower, double upper);
	void set_size (Rect const&);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
	
private:
	const Metric& _metric;
	Rect          _rect;

	/* lower and upper and sample positions, which are also canvas coordinates
	 */

	Coord         _lower;
	Coord         _upper;
};

}


#endif
