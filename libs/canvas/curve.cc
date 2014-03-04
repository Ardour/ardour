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

#include <exception>
#include <algorithm>

#include "canvas/curve.h"

using namespace ArdourCanvas;
using std::min;
using std::max;

Curve::Curve (Group* parent)
	: Item (parent)
	, PolyItem (parent)
	, Fill (parent)
	, n_samples (0)
	, n_segments (512)
{
	set_n_samples (256);
}

/** Set the number of points to compute when we smooth the data points into a
 * curve. 
 */
void
Curve::set_n_samples (Points::size_type n)
{
	/* this only changes our appearance rather than the bounding box, so we
	   just need to schedule a redraw rather than notify the parent of any
	   changes
	*/
	n_samples = n;
	samples.assign (n_samples, Duple (0.0, 0.0));
	interpolate ();
}

/** When rendering the curve, we will always draw a fixed number of straight
 * line segments to span the x-axis extent of the curve. More segments:
 * smoother visual rendering. Less rendering: closer to a visibily poly-line
 * render.
 */
void
Curve::set_n_segments (uint32_t n)
{
	/* this only changes our appearance rather than the bounding box, so we
	   just need to schedule a redraw rather than notify the parent of any
	   changes
	*/
	n_segments = n;
	redraw ();
}

void
Curve::compute_bounding_box () const
{
	PolyItem::compute_bounding_box ();

	/* possibly add extents of any point indicators here if we ever do that */
}

void
Curve::set (Points const& p)
{
	PolyItem::set (p);
	interpolate ();
}

void
Curve::interpolate ()
{
	Points::size_type npoints = _points.size ();

	if (npoints < 3) {
		return;
	}

	Duple p;
	double boundary;

	const double xfront = _points.front().x;
	const double xextent = _points.back().x - xfront;

	/* initialize boundary curve points */

	p = _points.front();
	boundary = round (((p.x - xfront)/xextent) * (n_samples - 1));

	for (Points::size_type i = 0; i < boundary; ++i) {
		samples[i] = Duple (i, p.y);
	}

	p = _points.back();
	boundary = round (((p.x - xfront)/xextent) * (n_samples - 1));

	for (Points::size_type i = boundary; i < n_samples; ++i) {
		samples[i] = Duple (i, p.y);
	}

	for (int i = 0; i < (int) npoints - 1; ++i) {

		Points::size_type p1, p2, p3, p4;
		
		p1 = max (i - 1, 0);
		p2 = i;
		p3 = i + 1;
		p4 = min (i + 2, (int) npoints - 1);

		smooth (p1, p2, p3, p4, xfront, xextent);
	}
	
	/* make sure that actual data points are used with their exact values */

	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {
		uint32_t idx = (((*p).x - xfront)/xextent) * (n_samples - 1);
		samples[idx].y = (*p).y;
	}
}

/*
 * This function calculates the curve values between the control points
 * p2 and p3, taking the potentially existing neighbors p1 and p4 into
 * account.
 *
 * This function uses a cubic bezier curve for the individual segments and
 * calculates the necessary intermediate control points depending on the
 * neighbor curve control points.
 *
 */

void
Curve::smooth (Points::size_type p1, Points::size_type p2, Points::size_type p3, Points::size_type p4,
	       double xfront, double xextent)
{
	int i;
	double x0, x3;
	double y0, y1, y2, y3;
	double dx, dy;
	double slope;

	/* the outer control points for the bezier curve. */

	x0 = _points[p2].x;
	y0 = _points[p2].y;
	x3 = _points[p3].x;
	y3 = _points[p3].y;

	/*
	 * the x values of the inner control points are fixed at
	 * x1 = 2/3*x0 + 1/3*x3   and  x2 = 1/3*x0 + 2/3*x3
	 * this ensures that the x values increase linearily with the
	 * parameter t and enables us to skip the calculation of the x
	 * values altogehter - just calculate y(t) evenly spaced.
	 */

	dx = x3 - x0;
	dy = y3 - y0;

	if (dx <= 0) {
		/* error? */
		return;
	}

	if (p1 == p2 && p3 == p4) {
		/* No information about the neighbors,
		 * calculate y1 and y2 to get a straight line
		 */
		y1 = y0 + dy / 3.0;
		y2 = y0 + dy * 2.0 / 3.0;

	} else if (p1 == p2 && p3 != p4) {

		/* only the right neighbor is available. Make the tangent at the
		 * right endpoint parallel to the line between the left endpoint
		 * and the right neighbor. Then point the tangent at the left towards
		 * the control handle of the right tangent, to ensure that the curve
		 * does not have an inflection point.
		 */
		slope = (_points[p4].y - y0) / (_points[p4].x - x0);

		y2 = y3 - slope * dx / 3.0;
		y1 = y0 + (y2 - y0) / 2.0;

	} else if (p1 != p2 && p3 == p4) {

		/* see previous case */
		slope = (y3 - _points[p1].y) / (x3 - _points[p1].x);

		y1 = y0 + slope * dx / 3.0;
		y2 = y3 + (y1 - y3) / 2.0;


	} else /* (p1 != p2 && p3 != p4) */ {

		/* Both neighbors are available. Make the tangents at the endpoints
		 * parallel to the line between the opposite endpoint and the adjacent
		 * neighbor.
		 */

		slope = (y3 - _points[p1].y) / (x3 - _points[p1].x);

		y1 = y0 + slope * dx / 3.0;

		slope = (_points[p4].y - y0) / (_points[p4].x - x0);

		y2 = y3 - slope * dx / 3.0;
	}

	/*
	 * finally calculate the y(t) values for the given bezier values. We can
	 * use homogenously distributed values for t, since x(t) increases linearily.
	 */

	dx = dx / xextent;

	int limit = round (dx * (n_samples - 1));
	const int idx_offset = round (((x0 - xfront)/xextent) * (n_samples - 1));

	for (i = 0; i <= limit; i++) {
		double y, t;
		Points::size_type index;

		t = i / dx / (n_samples - 1);
		
		y =     y0 * (1-t) * (1-t) * (1-t) +
			3 * y1 * (1-t) * (1-t) * t     +
			3 * y2 * (1-t) * t     * t     +
			y3 * t     * t     * t;

		index = i + idx_offset;
		
		if (index < n_samples) {
			Duple d (i, max (y, 0.0));
			samples[index] = d;
		}
	}
}

/** Given a fractional position within the x-axis range of the
 * curve, return the corresponding y-axis value
 */

double
Curve::map_value (double x) const
{
	if (x > 0.0 && x < 1.0) {

		double f;
		Points::size_type index;
		
		/* linearly interpolate between two of our smoothed "samples"
		 */
		
		x = x * (n_samples - 1);
		index = (Points::size_type) x; // XXX: should we explicitly use floor()?
		f = x - index;

		return (1.0 - f) * samples[index].y + f * samples[index+1].y;
		
	} else if (x >= 1.0) {
		return samples.back().y;
	} else {
		return samples.front().y;
	}
}

void
Curve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_outline || _points.size() < 2) {
		return;
	}

	/* Our approach is to always draw n_segments across our total size.
	 *
	 * This is very inefficient if we are asked to only draw a small
	 * section of the curve. For now we rely on cairo clipping to help
	 * with this.
	 */
	
	double y;

	if (!_bounding_box) {
		return;
	}
	Rect draw = area;

	setup_outline_context (context);
/*
	double r, g, b;
	r = (random() % 255) /  255.0;
	g = (random() % 255) /  255.0;
	b = (random() % 255) /  255.0;
	context->set_source_rgb (r, g, b);
*/
	if (_points.size() == 2) {

		/* straight line */

		Duple window_space;

		window_space = item_to_window (_points.front());
		context->move_to (window_space.x, window_space.y);
		window_space = item_to_window (_points.back());
		context->line_to (window_space.x, window_space.y);

	} else {

		/* curve of at least 3 points */
		
		/* clamp actual draw to area bound by points, rather than our bounding box which is slightly different */

		Duple w1 = item_to_window (Duple (_points.front().x, 0.0));
		Duple w2 = item_to_window (Duple (_points.back().x, 0.0));

		if (draw.x0 < w1.x) {
			draw.x0 = w1.x;
		}

		if (draw.x1 >= w2.x) {
			draw.x1 = w2.x;
		}

		std::cerr << "Draw " << draw << " win-x = " << w1 << " .. " << w2 << std::endl;

		/* full width of the curve */
		const double xextent = _points.back().x - _points.front().x;
		/* Determine where the first drawn point will be */
		Duple item_space = window_to_item (Duple (draw.x0, 0)); /* y value is irrelevant */
		/* determine the fractional offset of this location into the overall extent of the curve */
		const double xfract_offset = (item_space.x - _points.front().x)/xextent;
		const uint32_t pixels = draw.width ();
		Duple window_space;

#if 1
		std::cerr << "begin draw at " << draw.x0 << " (" << item_space.x << ") which is " << xfract_offset << " into " << w1.x << " .. " << w2.x 
			  << " I = " << _points.front().x << " .. " << _points.back().x 
			  << std::endl;
#endif

		/* draw the first point */

		std::cerr << this << " redraw " << pixels << " pixels from " << draw.x0 << " .. " << draw.x0 + pixels << std::endl;
		
		for (uint32_t pixel = 0; pixel < pixels; ++pixel) {

			/* fractional distance into the total horizontal extent of the curve */
			double xfract = xfract_offset + (pixel / xextent);
			/* compute vertical coordinate (item-space) at that location */
			y = map_value (xfract);
			
			/* convert to window space for drawing */
			window_space = item_to_window (Duple (0.0, y)); /* x-value is irrelevant */

			/* we are moving across the draw area pixel-by-pixel */
			window_space.x = draw.x0 + pixel;
			
			/* plot this point */
			if (pixel == 0) {
				context->move_to (window_space.x, window_space.y);
			} else {
				context->line_to (window_space.x, window_space.y);
			}
			std::cerr << window_space << ' ';
		}
		std::cerr << std::endl;
	}

	context->stroke ();

#if 0
	/* add points */
	
	setup_fill_context (context);
	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {
		Duple window_space (item_to_window (*p));
		context->arc (window_space.x, window_space.y, 5.0, 0.0, 2 * M_PI);
		context->stroke ();
	}
#endif
}

bool
Curve::covers (Duple const & pc) const
{
	Duple point = canvas_to_item (pc);

	/* O(N) N = number of points, and not accurate */

	for (Points::const_iterator p = _points.begin(); p != _points.end(); ++p) {

		const Coord dx = point.x - (*p).x;
		const Coord dy = point.y - (*p).y;
		const Coord dx2 = dx * dx;
		const Coord dy2 = dy * dy;

		if ((dx2 < 2.0 && dy2 < 2.0) || (dx2 + dy2 < 4.0)) {
			return true;
		}
	}

	return false;
}
