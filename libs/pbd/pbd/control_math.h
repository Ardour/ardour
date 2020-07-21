/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_control_math_h__
#define __pbd_control_math_h__

#include <assert.h>
#include <math.h>
#include <stdint.h>

/* these numbers ar arbitrary; we use them to keep floats well out of the denormal range */
#define TINY_NUMBER (0.0000001)  /* (-140dB) */

/* map gain-coeff [0..2] to position [0..1] */
static inline double
gain_to_position (double g)
{
	if (g == 0) {
		return 0;
	}
	return pow ((6.0 * log (g) / log (2.0) + 192.0) / 198.0, 8.0);
}

/* map position [0..1] to gain-coeff [0..2] */
static inline double
position_to_gain (double pos)
{
	if (pos == 0.0) {
		return 0.0;
	}
	return exp (((pow (pos, 1.0 / 8.0) * 198.0) - 192.0) / 6.0 * log (2.0));
}

/* map position [0..1] to parameter [lower..upper] on a logarithmic scale */
static inline double
position_to_logscale (double pos, double lower, double upper)
{
	assert (upper > lower && lower * upper > 0);
	assert (pos >= 0.0 && pos <= 1.0);
	return lower * pow (upper / lower, pos);
}

/* map parameter [lower..upper] to position [0..1] on a logarithmic scale*/
static inline double
logscale_to_position (double val, double lower, double upper)
{
	assert (upper > lower && lower * upper > 0);
	assert (val >= lower && val <= upper);
	return log (val / lower) / log (upper / lower);
}

static inline double
logscale_to_position_with_steps (double val, double lower, double upper, uint32_t steps)
{
	assert (steps > 1);
	double v = logscale_to_position (val, lower, upper) * (steps - 1.0);
	return round (v) / (steps - 1.0);
}

static inline double
position_to_logscale_with_steps (double pos, double lower, double upper, uint32_t steps)
{
	assert (steps > 1);
	double p = round (pos * (steps - 1.0)) / (steps - 1.0);
	return position_to_logscale (p, lower, upper);
}


static inline double
interpolate_linear (double from, double to, double fraction)
{
	return from + (fraction * (to - from));
}

static inline double
interpolate_logarithmic (double from, double to, double fraction, double /*lower*/, double /*upper*/)
{
#if 0
	/* this is expensive, original math incl. range-check assertions */
	double l0 = logscale_to_position (from, lower, upper);
	double l1 = logscale_to_position (to, lower, upper);
	return position_to_logscale (l0 + fraction * (l1 - l0), lower, upper);
#else
	assert (from > 0 && from * to > 0);
	assert (fraction >= 0 && fraction <= 1);
	return from * pow (to / from, fraction);
#endif
}

static inline double
interpolate_gain (double f, double t, double fraction, double upper)
{
	double from = f + TINY_NUMBER; //kill denormals before we use them for anything
	double to = t + TINY_NUMBER; //kill denormals before we use them for anything
	if ( fabs(to-from) < TINY_NUMBER ){
		 return to;
	}

	// this is expensive -- optimize
	double g0 = gain_to_position (from * 2. / upper);
	double g1 = gain_to_position (to * 2. / upper);
	double diff = g1 - g0;

	return position_to_gain (g0 + fraction * (diff)) * upper / 2.;
}

#endif
