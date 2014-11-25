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

#include "canvas/visibility.h"

#include "canvas/interpolated_curve.h"
#include "canvas/poly_item.h"
#include "canvas/fill.h"

namespace ArdourCanvas {

class XFadeCurve;

class LIBCANVAS_API Curve : public PolyItem, public InterpolatedCurve
{
  public:
    Curve (Canvas*);
    Curve (Item*);

    enum CurveFill {
	    None,
	    Inside,
	    Outside,
    };

    void compute_bounding_box () const;
    void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
    void set (Points const &);

    void set_points_per_segment (uint32_t n);

    bool covers (Duple const &) const;
    void set_fill_mode (CurveFill cf) { curve_fill = cf; }

  private:
    Points samples;
    Points::size_type n_samples;
    uint32_t points_per_segment;
    CurveFill curve_fill;

    void interpolate ();
};

}

#endif
