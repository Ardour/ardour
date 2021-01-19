/*
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <float.h>
#include <cmath>
#include <climits>
#include <cfloat>
#include <cmath>
#include <vector>

#include <glibmm/threads.h>

#include "pbd/control_math.h"

#include "evoral/Curve.h"
#include "evoral/ControlList.h"

using namespace std;
using namespace sigc;

namespace Evoral {


Curve::Curve (const ControlList& cl)
	: _dirty (true)
	, _list (cl)
{
}

void
Curve::solve () const
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

		vector<Temporal::timepos_t> x (npoints);
		vector<double> y(npoints);
		uint32_t i;
		ControlList::EventList::const_iterator xx;

		for (i = 0, xx = _list.events().begin(); xx != _list.events().end(); ++xx, ++i) {
			x[i] = (*xx)->when;
			y[i] = (*xx)->value;
		}

		double lp0, lp1, fpone;
		double xd;

		xd = x[0].distance (x[1]).magnitude();
		lp0 = xd/(y[1] - y[0]);
		xd = x[1].distance (x[2]).magnitude();
		lp1 = xd/(y[2] - y[1]);

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
			double xi = x[i].val();
			double xim1;

			if (i > 0) {
				xim1 = x[i-1].val();
				xdelta = xi - xim1;
				xdelta2 = xdelta * xdelta;
				ydelta = y[i] - y[i-1];
			}

			/* compute (constrained) first derivatives */

			if (i == 0) {

				/* first segment */

				fplast = ((3 * (y[1] - y[0]) / (2 * (xi - xim1))) - (fpone * 0.5));

				/* we don't store coefficients for i = 0 */

				continue;

			} else if (i == npoints - 1) {

				/* last segment */

				fpi = ((3 * ydelta) / (2 * xdelta)) - (fplast * 0.5);

			} else {

				/* all other segments */

				double xip1 = x[i+1].val();

				double slope_before = (xip1 - xi) / (y[i+1] - y[i]);
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
			c = ((xi * fppL) - (xim1 * fppR))/(2 * xdelta);

			double xim12, xim13;
			double xi2, xi3;

			xim12 = xim1 * xim1;  /* "x[i-1] squared" */
			xim13 = xim12 * xim1;   /* "x[i-1] cubed" */
			xi2 = xi * xi;        /* "x[i] squared" */
			xi3 = xi2 * xi;         /* "x[i] cubed" */

			b = (ydelta - (c * (xi2 - xim12)) - (d * (xi3 - xim13))) / xdelta;

			/* store */

			(*xx)->create_coeffs();
			(*xx)->coeff[0] = y[i-1] - (b * xim1) - (c * xim12) - (d * xim13);
			(*xx)->coeff[1] = b;
			(*xx)->coeff[2] = c;
			(*xx)->coeff[3] = d;

			fplast = fpi;
		}

	}

	_dirty = false;
}

bool
Curve::rt_safe_get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *vec, int32_t veclen) const
{
	Glib::Threads::RWLock::ReaderLock lm(_list.lock(), Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return false;
	} else {
		_get_vector (x0, x1, vec, veclen);
		return true;
	}
}

void
Curve::get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *vec, int32_t veclen) const
{
	Glib::Threads::RWLock::ReaderLock lm(_list.lock());
	_get_vector (x0, x1, vec, veclen);
}

void
Curve::_get_vector (Temporal::timepos_t const & x0, Temporal::timepos_t const & x1, float *vec, int32_t veclen) const
{
	double rx, lx, hx;
	const double start = x0.val();
	const double end = x1.val();
	double max_x;
	double min_x;
	int32_t i;
	int32_t original_veclen;
	int32_t npoints;

	if (veclen == 0) {
		return;
	}

	if ((npoints = _list.events().size()) == 0) {
		/* no events in list, so just fill the entire array with the default value */
		for (int32_t i = 0; i < veclen; ++i) {
			vec[i] = _list.descriptor().normal;
		}
		return;
	}

	if (npoints == 1) {
		for (int32_t i = 0; i < veclen; ++i) {
			vec[i] = _list.events().front()->value;
		}
		return;
	}

	/* events is now known not to be empty */

	max_x = _list.events().back()->when.val();
	min_x = _list.events().front()->when.val();

	if (start > max_x) {
		/* totally past the end - just fill the entire array with the final value */
		for (int32_t i = 0; i < veclen; ++i) {
			vec[i] = _list.events().back()->value;
		}
		return;
	}

	if (end < min_x) {
		/* totally before the first event - fill the entire array with
		 * the initial value.
		 */
		for (int32_t i = 0; i < veclen; ++i) {
			vec[i] = _list.events().front()->value;
		}
		return;
	}

	original_veclen = veclen;

	if (start < min_x) {

		/* fill some beginning section of the array with the
		   initial (used to be default) value
		*/

		double frac = (min_x - start) / (end - start);
		int64_t fill_len = (int64_t) floor (veclen * frac);

		fill_len = min (fill_len, (int64_t)veclen);

		for (i = 0; i < fill_len; ++i) {
			vec[i] = _list.events().front()->value;
		}

		veclen -= fill_len;
		vec += fill_len;
	}

	if (veclen && end > max_x) {

		/* fill some end section of the array with the default or final value */

		double frac = (end - max_x) / (end - start);
		int64_t fill_len = (int64_t) floor (original_veclen * frac);
		float val;

		fill_len = min (fill_len, (int64_t)veclen);
		val = _list.events().back()->value;

		for (i = veclen - fill_len; i < veclen; ++i) {
			vec[i] = val;
		}

		veclen -= fill_len;
	}

	lx = max (min_x, start);
	hx = min (max_x, end);

	if (npoints == 2) {

		const double lpos = _list.events().front()->when.val();
		const double lval = _list.events().front()->value;
		const double upos = _list.events().back()->when.val();
		const double uval = _list.events().back()->value;

		/* dx that we are using */
		if (veclen > 1) {
			const double dx_num = hx - lx;
			const double dx_den = veclen - 1;
			const double lower = _list.descriptor().lower;
			const double upper = _list.descriptor().upper;

			/* gradient of the line */
			const double m_num = uval - lval;
			const double m_den = upos - lpos;
			/* y intercept of the line */
			const double c = uval - (m_num * upos / m_den);

			switch (_list.interpolation()) {
				case ControlList::Logarithmic:
					for (int i = 0; i < veclen; ++i) {
						const double fraction = (lx - lpos + i * dx_num / dx_den) / m_den;
						vec[i] = interpolate_logarithmic (lval, uval, fraction, lower, upper);
					}
					break;
				case ControlList::Exponential:
					for (int i = 0; i < veclen; ++i) {
						const double fraction = (lx - lpos + i * dx_num / dx_den) / m_den;
						vec[i] = interpolate_gain (lval, uval, fraction, upper);
					}
					break;
				case ControlList::Discrete:
					// any discrete vector curves somewhere?
					assert (0);
				case ControlList::Curved:
					/* no 2 point spline */
					/* fallthrough */
				default: // Linear:
					for (int i = 0; i < veclen; ++i) {
						vec[i] = (lx * (m_num / m_den) + m_num * i * dx_num / (m_den * dx_den)) + c;
					}
					break;
			}
		} else {
			double fraction = (lx - lpos) / (upos - lpos);
			switch (_list.interpolation()) {
				case ControlList::Logarithmic:
					vec[0] = interpolate_logarithmic (lval, uval, fraction, _list.descriptor().lower, _list.descriptor().upper);
					break;
				case ControlList::Exponential:
					vec[0] = interpolate_gain (lval, uval, fraction, _list.descriptor().upper);
					break;
				case ControlList::Discrete:
					// any discrete vector curves somewhere?
					assert (0);
				case ControlList::Curved:
					/* no 2 point spline */
					/* fallthrough */
				default: // Linear:
					vec[0] = interpolate_linear (lval, uval, fraction);
					break;
			}
		}

		return;
	}

	if (_dirty) {
		solve ();
	}

	rx = lx;

	double dx = 0.;

	if (veclen > 1) {
		dx = (hx - lx) / (veclen - 1);
	}

	for (i = 0; i < veclen; ++i, rx += dx) {
		vec[i] = multipoint_eval (x0.is_beats() ? Temporal::timepos_t::from_ticks (rx) : Temporal::timepos_t::from_superclock (rx));
	}
}

double
Curve::multipoint_eval (Temporal::timepos_t const & x) const
{
	pair<ControlList::EventList::const_iterator,ControlList::EventList::const_iterator> range;

	ControlList::LookupCache& lookup_cache = _list.lookup_cache();

	if ((lookup_cache.left == Temporal::timepos_t::max (_list.time_domain())) ||
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

		ControlEvent* after = (*range.second);
		range.second--;
		ControlEvent* before = (*range.second);

		double vdelta = after->value - before->value;

		if (vdelta == 0.0) {
			return before->value;
		}

		double aw = after->when.val();
		double bw = before->when.val();

		double tdelta = x.val() - bw;
		double trange = aw - bw;

		switch (_list.interpolation()) {
			case ControlList::Discrete:
				return before->value;
			case ControlList::Logarithmic:
				return interpolate_logarithmic (before->value, after->value, tdelta / trange, _list.descriptor().lower, _list.descriptor().upper);
			case ControlList::Exponential:
				return interpolate_gain (before->value, after->value, tdelta / trange, _list.descriptor().upper);
			case ControlList::Curved:
				if (after->coeff) {
					ControlEvent* ev = after;

					/* As of Jan 2020, we only use Curved
					 * for fade in/out curves (of audio
					 * regions).
					 *
					 * This means that x is a relatively
					 * small value (an offset into the
					 * fade) amd we do not need to worry
					 * about the square overflowing.
					 */

					double x2 = x.val() * x.val();
					return ev->coeff[0] + (ev->coeff[1] * x.val()) + (ev->coeff[2] * x2) + (ev->coeff[3] * x2 * x.val());
				}
				/* fallthrough */
			case ControlList::Linear:
				return before->value + (vdelta * (tdelta / trange));
		}
	}

	/* x is a control point in the data */
	/* invalidate the cached range because its not usable */
	lookup_cache.left = Temporal::timepos_t::max (_list.time_domain());
	return (*range.first)->value;
}

} // namespace Evoral
