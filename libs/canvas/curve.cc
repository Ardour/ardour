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
{

}

void
Curve::compute_bounding_box () const
{
	PolyItem::compute_bounding_box ();

	if (_bounding_box) {

		bool have1 = false;
		bool have2 = false;
		
		Rect bbox1;
		Rect bbox2;

		for (Points::const_iterator i = first_control_points.begin(); i != first_control_points.end(); ++i) {
			if (have1) {
				bbox1.x0 = min (bbox1.x0, i->x);
				bbox1.y0 = min (bbox1.y0, i->y);
				bbox1.x1 = max (bbox1.x1, i->x);
				bbox1.y1 = max (bbox1.y1, i->y);
			} else {
				bbox1.x0 = bbox1.x1 = i->x;
				bbox1.y0 = bbox1.y1 = i->y;
				have1 = true;
			}
		}
		
		for (Points::const_iterator i = second_control_points.begin(); i != second_control_points.end(); ++i) {
			if (have2) {
				bbox2.x0 = min (bbox2.x0, i->x);
				bbox2.y0 = min (bbox2.y0, i->y);
				bbox2.x1 = max (bbox2.x1, i->x);
				bbox2.y1 = max (bbox2.y1, i->y);
			} else {
				bbox2.x0 = bbox2.x1 = i->x;
				bbox2.y0 = bbox2.y1 = i->y;
				have2 = true;
			}
		}
		
		Rect u = bbox1.extend (bbox2);
		_bounding_box = u.extend (_bounding_box.get());
	}
	
	_bounding_box_dirty = false;
}

void
Curve::set (Points const& p)
{
	PolyItem::set (p);

	first_control_points.clear ();
	second_control_points.clear ();

	compute_control_points (_points, first_control_points, second_control_points);
}

void
Curve::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		context->stroke ();
	}
}

void 
Curve::render_path (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	std::cerr << whatami() << '/' << name << " render curve w/" << _points.size() << " points, " << first_control_points.size() << " first and "
		  << second_control_points.size() << " second\n";
	PolyItem::render_curve (area, context, first_control_points, second_control_points);
}

void 
Curve::compute_control_points (Points const& knots,
			       Points& firstControlPoints, 
			       Points& secondControlPoints)
{
	Points::size_type n = knots.size() - 1;
	
	if (n < 1) {
		return;
	}
	
	if (n == 1) { 
		/* Special case: Bezier curve should be a straight line. */
		
		Duple d;
		
		d.x = (2.0 * knots[0].x + knots[1].x) / 3;
		d.y = (2.0 * knots[0].y + knots[1].y) / 3;
		firstControlPoints.push_back (d);
		
		d.x = 2.0 * firstControlPoints[0].x - knots[0].x;
		d.y = 2.0 * firstControlPoints[0].y - knots[0].y;
		secondControlPoints.push_back (d);
		
		return;
	}
	
	// Calculate first Bezier control points
	// Right hand side vector
	
	std::vector<double> rhs;
	
	rhs.assign (n, 0);
	
	// Set right hand side X values
	
	for (Points::size_type i = 1; i < n - 1; ++i) {
		rhs[i] = 4 * knots[i].x + 2 * knots[i + 1].x;
	}
	rhs[0] = knots[0].x + 2 * knots[1].x;
	rhs[n - 1] = (8 * knots[n - 1].x + knots[n].x) / 2.0;
	
	// Get first control points X-values
	double* x = solve (rhs);

	// Set right hand side Y values
	for (Points::size_type i = 1; i < n - 1; ++i) {
		rhs[i] = 4 * knots[i].y + 2 * knots[i + 1].y;
	}
	rhs[0] = knots[0].y + 2 * knots[1].y;
	rhs[n - 1] = (8 * knots[n - 1].y + knots[n].y) / 2.0;
	
	// Get first control points Y-values
	double* y = solve (rhs);
	
	for (Points::size_type i = 0; i < n; ++i) {
		
		firstControlPoints.push_back (Duple (x[i], y[i]));
		
		if (i < n - 1) {
			secondControlPoints.push_back (Duple (2 * knots [i + 1].x - x[i + 1], 
							      2 * knots[i + 1].y - y[i + 1]));
		} else {
			secondControlPoints.push_back (Duple ((knots [n].x + x[n - 1]) / 2,
							      (knots[n].y + y[n - 1]) / 2));
		}
	}
	
	delete [] x;
	delete [] y;
}

/** Solves a tridiagonal system for one of coordinates (x or y)
 *  of first Bezier control points.
 */

double*  
Curve::solve (std::vector<double> const & rhs) 
{
	std::vector<double>::size_type n = rhs.size();
	double* x = new double[n]; // Solution vector.
	double* tmp = new double[n]; // Temp workspace.
	
	double b = 2.0;

	x[0] = rhs[0] / b;

	for (std::vector<double>::size_type i = 1; i < n; i++) {
		// Decomposition and forward substitution.
		tmp[i] = 1 / b;
		b = (i < n - 1 ? 4.0 : 3.5) - tmp[i];
		x[i] = (rhs[i] - x[i - 1]) / b;
	}
	
	for (std::vector<double>::size_type i = 1; i < n; i++) {
		// Backsubstitution
		x[n - i - 1] -= tmp[n - i] * x[n - i]; 
	}

	delete [] tmp;
	
	return x;
}

bool
Curve::covers (Duple const & point) const
{
	return false;
}
