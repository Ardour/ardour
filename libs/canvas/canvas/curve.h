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

#ifndef __CANVAS_CURVE_H__
#define __CANVAS_CURVE_H__

#include "canvas/poly_item.h"

namespace ArdourCanvas {

class Curve : public PolyItem
{
public:
    Curve (Group *);
    
    void compute_bounding_box () const;
    void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
    void set (Points const &);

    bool covers (Duple const &) const;

  protected:
    void render_path (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
    void render_curve (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
    
  private:
    Points first_control_points;
    Points second_control_points;

    
    static void compute_control_points (Points const &,
					Points&, Points&);
    static double* solve (std::vector<double> const&);
};
	
}

#endif
