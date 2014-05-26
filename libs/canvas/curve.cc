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

#include <cmath>
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
	, points_per_segment (16)
	, curve_type (CatmullRomCentripetal)
{
}

/** When rendering the curve, we will always draw a fixed number of straight
 * line segments to span the x-axis extent of the curve. More segments:
 * smoother visual rendering. Less rendering: closer to a visibily poly-line
 * render.
 */
void
Curve::set_points_per_segment (uint32_t n)
{
	/* this only changes our appearance rather than the bounding box, so we
	   just need to schedule a redraw rather than notify the parent of any
	   changes
	*/
	points_per_segment = n;
	interpolate ();
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
	samples.clear ();
	interpolate (_points, points_per_segment, CatmullRomCentripetal, false, samples);
	n_samples = samples.size();
}

/* Cartmull-Rom code from http://stackoverflow.com/questions/9489736/catmull-rom-curve-with-no-cusps-and-no-self-intersections/19283471#19283471
 * 
 * Thanks to Ted for his Java version, which I translated into Ardour-idiomatic
 * C++ here.
 */

/**
 * Calculate the same values but introduces the ability to "parameterize" the t
 * values used in the calculation. This is based on Figure 3 from
 * http://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf
 *
 * @param p An array of double values of length 4, where interpolation
 * occurs from p1 to p2.
 * @param time An array of time measures of length 4, corresponding to each
 * p value.
 * @param t the actual interpolation ratio from 0 to 1 representing the
 * position between p1 and p2 to interpolate the value.
 */
static double 
__interpolate (double p[4], double time[4], double t) 
{
        const double L01 = p[0] * (time[1] - t) / (time[1] - time[0]) + p[1] * (t - time[0]) / (time[1] - time[0]);
        const double L12 = p[1] * (time[2] - t) / (time[2] - time[1]) + p[2] * (t - time[1]) / (time[2] - time[1]);
        const double L23 = p[2] * (time[3] - t) / (time[3] - time[2]) + p[3] * (t - time[2]) / (time[3] - time[2]);
        const double L012 = L01 * (time[2] - t) / (time[2] - time[0]) + L12 * (t - time[0]) / (time[2] - time[0]);
        const double L123 = L12 * (time[3] - t) / (time[3] - time[1]) + L23 * (t - time[1]) / (time[3] - time[1]);
        const double C12 = L012 * (time[2] - t) / (time[2] - time[1]) + L123 * (t - time[1]) / (time[2] - time[1]);
        return C12;
}   

/**
 * Given a list of control points, this will create a list of points_per_segment
 * points spaced uniformly along the resulting Catmull-Rom curve.
 *
 * @param points The list of control points, leading and ending with a 
 * coordinate that is only used for controling the spline and is not visualized.
 * @param index The index of control point p0, where p0, p1, p2, and p3 are
 * used in order to create a curve between p1 and p2.
 * @param points_per_segment The total number of uniformly spaced interpolated
 * points to calculate for each segment. The larger this number, the
 * smoother the resulting curve.
 * @param curve_type Clarifies whether the curve should use uniform, chordal
 * or centripetal curve types. Uniform can produce loops, chordal can
 * produce large distortions from the original lines, and centripetal is an
 * optimal balance without spaces.
 * @return the list of coordinates that define the CatmullRom curve
 * between the points defined by index+1 and index+2.
 */
static void
_interpolate (const Points& points, Points::size_type index, int points_per_segment, Curve::SplineType curve_type, Points& results) 
{
        double x[4];
        double y[4];
        double time[4];

        for (int i = 0; i < 4; i++) {
                x[i] = points[index + i].x;
                y[i] = points[index + i].y;
                time[i] = i;
        }
        
        double tstart = 1;
        double tend = 2;

        if (curve_type != Curve::CatmullRomUniform) {
                double total = 0;
                for (int i = 1; i < 4; i++) {
                        double dx = x[i] - x[i - 1];
                        double dy = y[i] - y[i - 1];
                        if (curve_type == Curve::CatmullRomCentripetal) {
                                total += pow (dx * dx + dy * dy, .25);
                        } else {
                                total += pow (dx * dx + dy * dy, .5);
                        }
                        time[i] = total;
                }
                tstart = time[1];
                tend = time[2];
        }

        int segments = points_per_segment - 1;
        results.push_back (points[index + 1]);

        for (int i = 1; i < segments; i++) {
                double xi = __interpolate (x, time, tstart + (i * (tend - tstart)) / segments);
                double yi = __interpolate (y, time, tstart + (i * (tend - tstart)) / segments);
                results.push_back (Duple (xi, yi));
        }

        results.push_back (points[index + 2]);
}

/**
 * This method will calculate the Catmull-Rom interpolation curve, returning
 * it as a list of Coord coordinate objects.  This method in particular
 * adds the first and last control points which are not visible, but required
 * for calculating the spline.
 *
 * @param coordinates The list of original straight line points to calculate
 * an interpolation from.
 * @param points_per_segment The integer number of equally spaced points to
 * return along each curve.  The actual distance between each
 * point will depend on the spacing between the control points.
 * @return The list of interpolated coordinates.
 * @param curve_type Chordal (stiff), Uniform(floppy), or Centripetal(medium)
 * @throws gov.ca.water.shapelite.analysis.CatmullRomException if
 * points_per_segment is less than 2.
 */

void
Curve::interpolate (const Points& coordinates, uint32_t points_per_segment, SplineType curve_type, bool closed, Points& results)
{
        if (points_per_segment < 2) {
                return;
        }
        
        // Cannot interpolate curves given only two points.  Two points
        // is best represented as a simple line segment.
        if (coordinates.size() < 3) {
                results = coordinates;
                return;
        }

        // Copy the incoming coordinates. We need to modify it during interpolation
        Points vertices = coordinates;
        
        // Test whether the shape is open or closed by checking to see if
        // the first point intersects with the last point.  M and Z are ignored.
        if (closed) {
                // Use the second and second from last points as control points.
                // get the second point.
                Duple p2 = vertices[1];
                // get the point before the last point
                Duple pn1 = vertices[vertices.size() - 2];
                
                // insert the second from the last point as the first point in the list
                // because when the shape is closed it keeps wrapping around to
                // the second point.
                vertices.insert(vertices.begin(), pn1);
                // add the second point to the end.
                vertices.push_back(p2);
        } else {
                // The shape is open, so use control points that simply extend
                // the first and last segments
                
                // Get the change in x and y between the first and second coordinates.
                double dx = vertices[1].x - vertices[0].x;
                double dy = vertices[1].y - vertices[0].y;
                
                // Then using the change, extrapolate backwards to find a control point.
                double x1 = vertices[0].x - dx;
                double y1 = vertices[0].y - dy;
                
                // Actaully create the start point from the extrapolated values.
                Duple start (x1, y1);
                
                // Repeat for the end control point.
                int n = vertices.size() - 1;
                dx = vertices[n].x - vertices[n - 1].x;
                dy = vertices[n].y - vertices[n - 1].y;
                double xn = vertices[n].x + dx;
                double yn = vertices[n].y + dy;
                Duple end (xn, yn);
                
                // insert the start control point at the start of the vertices list.
                vertices.insert (vertices.begin(), start);
                
                // append the end control ponit to the end of the vertices list.
                vertices.push_back (end);
        }
        
        // When looping, remember that each cycle requires 4 points, starting
        // with i and ending with i+3.  So we don't loop through all the points.
        
        for (Points::size_type i = 0; i < vertices.size() - 3; i++) {

                // Actually calculate the Catmull-Rom curve for one segment.
		Points r;

                _interpolate (vertices, i, points_per_segment, curve_type, r);
 
                // Since the middle points are added twice, once for each bordering
                // segment, we only add the 0 index result point for the first
                // segment.  Otherwise we will have duplicate points.

                if (results.size() > 0) {
                        r.erase (r.begin());
                }
                
                // Add the coordinates for the segment to the result list.

                results.insert (results.end(), r.begin(), r.end());
        }
}

void
Curve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_outline || _points.size() < 2 || !_bounding_box) {
		return;
	}

	Rect self = item_to_window (_bounding_box.get());
	boost::optional<Rect> d = self.intersection (area);
	assert (d);
	Rect draw = d.get ();

	/* Our approach is to always draw n_segments across our total size.
	 *
	 * This is very inefficient if we are asked to only draw a small
	 * section of the curve. For now we rely on cairo clipping to help
	 * with this.
	 */
	

	setup_outline_context (context);

	if (_points.size() == 2) {

		/* straight line */

		Duple window_space;

		window_space = item_to_window (_points.front());
		context->move_to (window_space.x, window_space.y);
		window_space = item_to_window (_points.back());
		context->line_to (window_space.x, window_space.y);

		context->stroke ();

	} else {

		/* curve of at least 3 points */

		/* x-axis limits of the curve, in window space coordinates */

		Duple w1 = item_to_window (Duple (_points.front().x, 0.0));
		Duple w2 = item_to_window (Duple (_points.back().x, 0.0));

		/* clamp actual draw to area bound by points, rather than our bounding box which is slightly different */

		context->save ();
		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->clip ();

		/* expand drawing area by several pixels on each side to avoid cairo stroking effects at the boundary.
		   they will still occur, but cairo's clipping will hide them.
		 */

		draw = draw.expand (4.0);

		/* now clip it to the actual points in the curve */
		
		if (draw.x0 < w1.x) {
			draw.x0 = w1.x;
		}

		if (draw.x1 >= w2.x) {
			draw.x1 = w2.x;
		}

		/* find left and right-most sample */
		Points::size_type left = 0;
		Points::size_type right = n_samples;

		for (Points::size_type idx = 0; idx < n_samples - 1; ++idx) {
			left = idx;
			if (samples[idx].x >= draw.x0) break;
		}
		for (Points::size_type idx = n_samples; idx > left + 1; --idx) {
			if (samples[idx].x <= draw.x1) break;
			right = idx;
		}

		/* draw line between samples */
		Duple window_space;
		window_space = item_to_window (Duple (samples[left].x, samples[left].y));
		context->move_to (window_space.x, window_space.y);
		for (uint32_t idx = left + 1; idx < right; ++idx) {
			window_space = item_to_window (Duple (samples[idx].x, samples[idx].y));
			context->line_to (window_space.x, window_space.y);
		}

		context->stroke ();
		context->restore ();
	}

#if 1
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
