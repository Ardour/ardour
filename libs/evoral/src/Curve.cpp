/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <iostream>
#include <float.h>
#include <cmath>
#include <climits>
#include <cfloat>
#include <cmath>

#include <glibmm/threads.h>

#include "evoral/Curve.hpp"
#include "evoral/ControlList.hpp"

using namespace std;
using namespace sigc;

namespace Evoral {


Curve::Curve (const ControlList& cl)
	: _dirty (true)
	, _list (cl)
{
}

void
Curve::solve ()
{
	uint32_t npoints;

	if (!_dirty) {
		return;
	}

	if ((npoints = _list.events().size()) > 2) {

		/* Compute coefficients needed to efficiently compute a constrained spline
		   curve. See "Constrained Cubic Spline Interpolation" by CJC Kruger
		   (www.korf.co.uk/spline.pdf) for more details.
		*/

		double x[npoints];
		double y[npoints];
		uint32_t i;
		ControlList::EventList::const_iterator xx;

		for (i = 0, xx = _list.events().begin(); xx != _list.events().end(); ++xx, ++i) {
			x[i] = (double) (*xx)->when;
			y[i] = (double) (*xx)->value;
		}

		double lp0, lp1, fpone;

		lp0 = (x[1] - x[0])/(y[1] - y[0]);
		lp1 = (x[2] - x[1])/(y[2] - y[1]);

		if (lp0*lp1 < 0) {
			fpone = 0;
		} else {
			fpone = 2 / (lp1 + lp0);
		}

		double fplast = 0;

		for (i = 0, xx = _list.events().begin(); xx != _list.events().end(); ++xx, ++i) {

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

			(*xx)->create_coeffs();
			(*xx)->coeff[0] = y[i-1] - (b * x[i-1]) - (c * xim12) - (d * xim13);
			(*xx)->coeff[1] = b;
			(*xx)->coeff[2] = c;
			(*xx)->coeff[3] = d;

			fplast = fpi;
		}

	}

	_dirty = false;
}

bool
Curve::rt_safe_get_vector (double x0, double x1, float *vec, int32_t veclen)
{
	Glib::Threads::Mutex::Lock lm(_list.lock(), Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return false;
	} else {
		_get_vector (x0, x1, vec, veclen);
		return true;
	}
}

void
Curve::get_vector (double x0, double x1, float *vec, int32_t veclen)
{
	Glib::Threads::Mutex::Lock lm(_list.lock());
	_get_vector (x0, x1, vec, veclen);
}

void
Curve::_get_vector (double x0, double x1, float *vec, int32_t veclen)
{
	double rx, lx, hx, max_x, min_x;
	int32_t i;
	int32_t original_veclen;
	int32_t npoints;

	if ((npoints = _list.events().size()) == 0) {
		for (i = 0; i < veclen; ++i) {
			vec[i] = _list.default_value();
		}
		return;
	}

	/* events is now known not to be empty */

	max_x = _list.events().back()->when;
	min_x = _list.events().front()->when;

	lx = max (min_x, x0);

	if (x1 < 0) {
		x1 = _list.events().back()->when;
	}

	hx = min (max_x, x1);

	original_veclen = veclen;

	if (x0 < min_x) {

		/* fill some beginning section of the array with the
		   initial (used to be default) value
		*/

		double frac = (min_x - x0) / (x1 - x0);
		int64_t subveclen = (int64_t) floor (veclen * frac);

		subveclen = min (subveclen, (int64_t)veclen);

		for (i = 0; i < subveclen; ++i) {
			vec[i] = _list.events().front()->value;
		}

		veclen -= subveclen;
		vec += subveclen;
	}

	if (veclen && x1 > max_x) {

		/* fill some end section of the array with the default or final value */

		double frac = (x1 - max_x) / (x1 - x0);

		int64_t subveclen = (int64_t) floor (original_veclen * frac);

		float val;

		subveclen = min (subveclen, (int64_t)veclen);

		val = _list.events().back()->value;

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
			vec[i] = _list.events().front()->value;
		}
		return;
	}


	if (npoints == 2) {

		/* linear interpolation between 2 points */

		/* XXX: this numerator / denominator stuff is pretty grim, but it's the only
		   way I could get the maths to be accurate; doing everything with pure doubles
		   gives ~1e-17 errors in the vec[i] computation.
		*/

		/* gradient of the line */
		double const m_num = _list.events().back()->value - _list.events().front()->value;
		double const m_den = _list.events().back()->when - _list.events().front()->when;

		/* y intercept of the line */
		double const c = double (_list.events().back()->value) - (m_num * _list.events().back()->when / m_den);

		/* dx that we are using */
		double dx_num = 0;
		double dx_den = 1;
		if (veclen > 1) {
			dx_num = hx - lx;
			dx_den = veclen - 1;
		}

		if (veclen > 1) {
			for (int i = 0; i < veclen; ++i) {
				vec[i] = (lx * (m_num / m_den) + m_num * i * dx_num / (m_den * dx_den)) + c;
			}
		} else {
			vec[0] = lx;
		}

		return;
	}

	if (_dirty) {
		solve ();
	}

	rx = lx;

	double dx = 0;
	if (veclen > 1) {
		dx = (hx - lx) / (veclen - 1);
	}

	for (i = 0; i < veclen; ++i, rx += dx) {
		vec[i] = multipoint_eval (rx);
	}
}

double
Curve::unlocked_eval (double x)
{
	// I don't see the point of this...

	if (_dirty) {
		solve ();
	}

	return _list.unlocked_eval (x);
}

double
Curve::multipoint_eval (double x)
{
	pair<ControlList::EventList::const_iterator,ControlList::EventList::const_iterator> range;

	ControlList::LookupCache& lookup_cache = _list.lookup_cache();

	if ((lookup_cache.left < 0) ||
	    ((lookup_cache.left > x) ||
	     (lookup_cache.range.first == _list.events().end()) ||
	     ((*lookup_cache.range.second)->when < x))) {

		ControlEvent cp (x, 0.0);

		lookup_cache.range = equal_range (_list.events().begin(), _list.events().end(), &cp, ControlList::time_comparator);
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

		if (range.first == _list.events().begin()) {
			/* we're before the first point */
			// return default_value;
			return _list.events().front()->value;
		}

		if (range.second == _list.events().end()) {
			/* we're after the last point */
			return _list.events().back()->value;
		}

		double x2 = x * x;
		ControlEvent* ev = *range.second;

		return ev->coeff[0] + (ev->coeff[1] * x) + (ev->coeff[2] * x2) + (ev->coeff[3] * x2 * x);
	}

	/* x is a control point in the data */
	/* invalidate the cached range because its not usable */
	lookup_cache.left = -1;
	return (*range.first)->value;
}

} // namespace Evoral

extern "C" {

void
curve_get_vector_from_c (void *arg, double x0, double x1, float* vec, int32_t vecsize)
{
	static_cast<Evoral::Curve*>(arg)->get_vector (x0, x1, vec, vecsize);
}

}
