/*
    Copyright (C) 2001-2003 Paul Davis 

    Contains ideas derived from "Constrained Cubic Spline Interpolation" 
    by CJC Kruger (www.korf.co.uk/spline.pdf).

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

#include <iostream>
#include <float.h>
#include <cmath>
#include <climits>
#include <cfloat>
#include <cmath>

#include <glibmm/thread.h>
#include <sigc++/bind.h>

#include "ardour/curve.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

Curve::Curve (double minv, double maxv, double canv, bool nostate)
	: AutomationList (canv)
{
	min_yval = minv;
	max_yval = maxv;
}

Curve::Curve (const Curve& other)
	: AutomationList (other)
{
	min_yval = other.min_yval;
	max_yval = other.max_yval;
}

Curve::Curve (const Curve& other, double start, double end)
	: AutomationList (other, start, end)
{
	min_yval = other.min_yval;
	max_yval = other.max_yval;
}

Curve::Curve (const XMLNode& node)
	: AutomationList (node)
{
}

Curve::~Curve ()
{
}

Curve&
Curve::operator= (const Curve& other)
{
	if (this != &other) {
		*((AutomationList*)this) = other;
		min_yval = other.min_yval;
		max_yval = other.max_yval;
	}
	return *this;
}

void
Curve::solve ()
{
	uint64_t npoints;

	if (!_dirty) {
		return;
	}
	
	if ((npoints = events.size()) > 2) {
		
		/* Compute coefficients needed to efficiently compute a constrained spline
		   curve. See "Constrained Cubic Spline Interpolation" by CJC Kruger
		   (www.korf.co.uk/spline.pdf) for more details.
		*/

		double x[npoints];
		double y[npoints];
		uint64_t i;
		AutomationEventList::iterator xx;

		for (i = 0, xx = events.begin(); xx != events.end(); ++xx, ++i) {
			x[i] = (double) (*xx)->when;
			y[i] = (double) (*xx)->value;
		}

		double lp0, lp1, fpone;

		lp0 =(x[1] - x[0])/(y[1] - y[0]);
		lp1 = (x[2] - x[1])/(y[2] - y[1]);

		if (lp0*lp1 < 0) {
			fpone = 0;
		} else {
			fpone = 2 / (lp1 + lp0);
		}

		double fplast = 0;

		for (i = 0, xx = events.begin(); xx != events.end(); ++xx, ++i) {
			
			CurvePoint* cp = dynamic_cast<CurvePoint*>(*xx);

			if (cp == 0) {
				fatal  << _("programming error: ")
				       << X_("non-CurvePoint event found in event list for a Curve")
				       << endmsg;
				/*NOTREACHED*/
			}
			
			double xdelta;   /* gcc is wrong about possible uninitialized use */
			double xdelta2;  /* ditto */
			double ydelta;   /* ditto */
			double fppL, fppR;
			double fpi;

			if (i > 0) {
				xdelta = x[i] - x[i-1];
				xdelta2 = xdelta * xdelta;
				ydelta = y[i] - y[i-1];
			}

			/* compute (constrained) first derivatives */
			
			if (i == 0) {

				/* first segment */
				
				fplast = ((3 * (y[1] - y[0]) / (2 * (x[1] - x[0]))) - (fpone * 0.5));

				/* we don't store coefficients for i = 0 */

				continue;

			} else if (i == npoints - 1) {

				/* last segment */

				fpi = ((3 * ydelta) / (2 * xdelta)) - (fplast * 0.5);
				
			} else {

				/* all other segments */

				double slope_before = ((x[i+1] - x[i]) / (y[i+1] - y[i]));
				double slope_after = (xdelta / ydelta);

				if (slope_after * slope_before < 0.0) {
					/* slope changed sign */
					fpi = 0.0;
				} else {
					fpi = 2 / (slope_before + slope_after);
				}
				
			}

			/* compute second derivative for either side of control point `i' */
			
			fppL = (((-2 * (fpi + (2 * fplast))) / (xdelta))) +
				((6 * ydelta) / xdelta2);
			
			fppR = (2 * ((2 * fpi) + fplast) / xdelta) -
				((6 * ydelta) / xdelta2);
			
			/* compute polynomial coefficients */

			double b, c, d;

			d = (fppR - fppL) / (6 * xdelta);   
			c = ((x[i] * fppL) - (x[i-1] * fppR))/(2 * xdelta);
			
			double xim12, xim13;
			double xi2, xi3;
			
			xim12 = x[i-1] * x[i-1];  /* "x[i-1] squared" */
			xim13 = xim12 * x[i-1];   /* "x[i-1] cubed" */
			xi2 = x[i] * x[i];        /* "x[i] squared" */
			xi3 = xi2 * x[i];         /* "x[i] cubed" */
			
			b = (ydelta - (c * (xi2 - xim12)) - (d * (xi3 - xim13))) / xdelta;

			/* store */

			cp->coeff[0] = y[i-1] - (b * x[i-1]) - (c * xim12) - (d * xim13);
			cp->coeff[1] = b;
			cp->coeff[2] = c;
			cp->coeff[3] = d;

			fplast = fpi;
		}
		
	}

	_dirty = false;
}

bool
Curve::rt_safe_get_vector (double x0, double x1, float *vec, int64_t veclen)
{
	Glib::Mutex::Lock lm (lock, Glib::TRY_LOCK);

	if (!lm.locked()) {
		return false;
	} else {
		_get_vector (x0, x1, vec, veclen);
		return true;
	}
}

void
Curve::get_vector (double x0, double x1, float *vec, int64_t veclen)
{
	Glib::Mutex::Lock lm (lock);
	_get_vector (x0, x1, vec, veclen);
}

void
Curve::_get_vector (double x0, double x1, float *vec, int64_t veclen)
{
	double rx, dx, lx, hx, max_x, min_x;
	int64_t i;
	int64_t original_veclen;
	int64_t npoints;

        if ((npoints = events.size()) == 0) {
                 for (i = 0; i < veclen; ++i) {
                         vec[i] = default_value;
                 }
                 return;
	}

	/* events is now known not to be empty */

	max_x = events.back()->when;
	min_x = events.front()->when;

	lx = max (min_x, x0);

	if (x1 < 0) {
		x1 = events.back()->when;
	}

	hx = min (max_x, x1);

	original_veclen = veclen;

	if (x0 < min_x) {

		/* fill some beginning section of the array with the 
		   initial (used to be default) value 
		*/

		double frac = (min_x - x0) / (x1 - x0);
		int64_t subveclen = (int64_t) floor (veclen * frac);
		
		subveclen = min (subveclen, veclen);

		for (i = 0; i < subveclen; ++i) {
			vec[i] = events.front()->value;
		}

		veclen -= subveclen;
		vec += subveclen;
	}

	if (veclen && x1 > max_x) {

		/* fill some end section of the array with the default or final value */

		double frac = (x1 - max_x) / (x1 - x0);

		int64_t subveclen = (int64_t) floor (original_veclen * frac);

		float val;
		
		subveclen = min (subveclen, veclen);

		val = events.back()->value;

		i = veclen - subveclen;

		for (i = veclen - subveclen; i < veclen; ++i) {
			vec[i] = val;
		}

		veclen -= subveclen;
	}

	if (veclen == 0) {
		return;
	}

 	if (npoints == 1) {
 	
 		for (i = 0; i < veclen; ++i) {
 			vec[i] = events.front()->value;
 		}
 		return;
 	}
 
 
 	if (npoints == 2) {
 
 		/* linear interpolation between 2 points */
 
 		/* XXX I'm not sure that this is the right thing to
 		   do here. but its not a common case for the envisaged
 		   uses.
 		*/
 	
 		if (veclen > 1) {
 			dx = (hx - lx) / (veclen - 1) ;
 		} else {
 			dx = 0; // not used
 		}
 	
 		double slope = (events.back()->value - events.front()->value)/  
			(events.back()->when - events.front()->when);
 		double yfrac = dx*slope;
 
 		vec[0] = events.front()->value + slope * (lx - events.front()->when);
 
 		for (i = 1; i < veclen; ++i) {
 			vec[i] = vec[i-1] + yfrac;
 		}
 
 		return;
 	}
 
	if (_dirty) {
		solve ();
	}

	rx = lx;

	/* note: if there are veclen elements in the output,
	   there are only veclen-1 steps between them.
	*/
	
	if (veclen > 1) {
		dx = (hx - lx) / (veclen-1);
	} else {
		dx = 0;
	}
	
	for (i = 0; i < veclen; ++i, rx += dx) {
		vec[i] = multipoint_eval (rx);
	}
}

double
Curve::unlocked_eval (double x)
{
	if (_dirty) {
		solve ();
	}

	return shared_eval (x);
}

double
Curve::multipoint_eval (double x)
{	
	pair<AutomationEventList::iterator,AutomationEventList::iterator> range;

	if ((lookup_cache.left < 0) ||
	    ((lookup_cache.left > x) || 
	     (lookup_cache.range.first == events.end()) || 
	     ((*lookup_cache.range.second)->when < x))) {
		
		TimeComparator cmp;
		ControlEvent cp (x, 0.0);

		lookup_cache.range = equal_range (events.begin(), events.end(), &cp, cmp);
	}

	range = lookup_cache.range;

	/* EITHER 
	   
	   a) x is an existing control point, so first == existing point, second == next point

	   OR

	   b) x is between control points, so range is empty (first == second, points to where
	       to insert x)
	   
	*/

	if (range.first == range.second) {

		/* x does not exist within the list as a control point */
		
		lookup_cache.left = x;

		if (range.first == events.begin()) {
			/* we're before the first point */
			// return default_value;
			return events.front()->value;
		}
		
		if (range.second == events.end()) {
			/* we're after the last point */
			return events.back()->value;
		}

		double x2 = x * x;
		CurvePoint* cp = dynamic_cast<CurvePoint*> (*range.second);

		return cp->coeff[0] + (cp->coeff[1] * x) + (cp->coeff[2] * x2) + (cp->coeff[3] * x2 * x);
	} 

	/* x is a control point in the data */
	/* invalidate the cached range because its not usable */
	lookup_cache.left = -1;
	return (*range.first)->value;
}

ControlEvent*
Curve::point_factory (double when, double val) const
{
	return new CurvePoint (when, val);
}

ControlEvent*
Curve::point_factory (const ControlEvent& other) const
{
	return new CurvePoint (other.when, other.value);
}

extern "C" {

void 
curve_get_vector_from_c (void *arg, double x0, double x1, float* vec, int64_t vecsize)
{
	static_cast<Curve*>(arg)->get_vector (x0, x1, vec, vecsize);
}

}
