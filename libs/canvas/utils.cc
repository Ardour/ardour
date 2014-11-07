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

#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <cairomm/context.h>

#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

void
ArdourCanvas::set_source_rgba (Cairo::RefPtr<Cairo::Context> context, Color color)
{
	context->set_source_rgba (
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		((color >>  0) & 0xff) / 255.0
		);
}

void
ArdourCanvas::set_source_rgb_a (Cairo::RefPtr<Cairo::Context> context, Color color, float alpha)
{
	context->set_source_rgba (
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		alpha
		);
}

void
ArdourCanvas::set_source_rgba (cairo_t *cr, Color color)
{
	cairo_set_source_rgba ( cr,
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		((color >>  0) & 0xff) / 255.0
		);
}

void
ArdourCanvas::set_source_rgb_a (cairo_t *cr, Color color, float alpha)
{
	cairo_set_source_rgba ( cr,
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		alpha
		);
}

ArdourCanvas::Distance
ArdourCanvas::distance_to_segment_squared (Duple const & p, Duple const & p1, Duple const & p2, double& t, Duple& at)
{
	static const double kMinSegmentLenSquared = 0.00000001;  // adjust to suit.  If you use float, you'll probably want something like 0.000001f
	static const double kEpsilon = 1.0E-14;  // adjust to suit.  If you use floats, you'll probably want something like 1E-7f
	double dx = p2.x - p1.x;
	double dy = p2.y - p1.y;
	double dp1x = p.x - p1.x;
	double dp1y = p.y - p1.y;
	const double segLenSquared = (dx * dx) + (dy * dy);

	if (segLenSquared >= -kMinSegmentLenSquared && segLenSquared <= kMinSegmentLenSquared) {
		// segment is a point.
		at = p1;
		t = 0.0;
		return ((dp1x * dp1x) + (dp1y * dp1y));
	} 


	// Project a line from p to the segment [p1,p2].  By considering the line
	// extending the segment, parameterized as p1 + (t * (p2 - p1)),
	// we find projection of point p onto the line. 
	// It falls where t = [(p - p1) . (p2 - p1)] / |p2 - p1|^2
		
	t = ((dp1x * dx) + (dp1y * dy)) / segLenSquared;

	if (t < kEpsilon) {
		// intersects at or to the "left" of first segment vertex (p1.x, p1.y).  If t is approximately 0.0, then
		// intersection is at p1.  If t is less than that, then there is no intersection (i.e. p is not within
		// the 'bounds' of the segment)
		if (t > -kEpsilon) {
			// intersects at 1st segment vertex
			t = 0.0;
		}
		// set our 'intersection' point to p1.
		at = p1;
		// Note: If you wanted the ACTUAL intersection point of where the projected lines would intersect if
		// we were doing PointLineDistanceSquared, then qx would be (p1.x + (t * dx)) and qy would be (p1.y + (t * dy)).

	} else if (t > (1.0 - kEpsilon)) {
		// intersects at or to the "right" of second segment vertex (p2.x, p2.y).  If t is approximately 1.0, then
		// intersection is at p2.  If t is greater than that, then there is no intersection (i.e. p is not within
		// the 'bounds' of the segment)
		if (t < (1.0 + kEpsilon)) {
			// intersects at 2nd segment vertex
			t = 1.0;
		}
		// set our 'intersection' point to p2.
		at = p2;
		// Note: If you wanted the ACTUAL intersection point of where the projected lines would intersect if
		// we were doing PointLineDistanceSquared, then qx would be (p1.x + (t * dx)) and qy would be (p1.y + (t * dy)).
	} else {
		// The projection of the point to the point on the segment that is perpendicular succeeded and the point
		// is 'within' the bounds of the segment.  Set the intersection point as that projected point.
		at = Duple (p1.x + (t * dx), p1.y + (t * dy));
	}

	// return the squared distance from p to the intersection point.  Note that we return the squared distance
	// as an optimization because many times you just need to compare relative distances and the squared values
	// works fine for that.  If you want the ACTUAL distance, just take the square root of this value.
	double dpqx = p.x - at.x;
	double dpqy = p.y - at.y;

	return ((dpqx * dpqx) + (dpqy * dpqy));
}

