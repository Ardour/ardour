/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_position_types_h__
#define __libpbd_position_types_h__

#include <cstdlib>
#include <stdint.h>

#include <boost/rational.hpp>

#include "pbd/integer_division.h"

namespace Temporal {

extern void init ();

/* Any position measured in audio samples.
   Assumed to be non-negative but not enforced.
*/
typedef int64_t samplepos_t;

/* Any distance from a given samplepos_t.
   Maybe positive or negative.
*/
typedef int64_t sampleoffset_t;

/* Any count of audio samples.
   Assumed to be positive but not enforced.
*/
typedef int64_t samplecnt_t;

static const samplepos_t max_samplepos = INT64_MAX;
static const samplecnt_t max_samplecnt = INT64_MAX;

/* This defines the smallest division of a "beat".

   The number is intended to have as many integer factors as possible so that
   1/Nth divisions are integer numbers of ticks.

   1920 has many factors, though going up to 3840 gets a couple more.
*/

static const int32_t ticks_per_beat = 1920;

template<class T>
class _ratio_t {
  public:
	/* do not allow negative values, this is just a ratio */

	_ratio_t (T n, T d) : _numerator (abs (n)), _denominator (abs(d)) { assert (_denominator != 0); }
	_ratio_t (T n) : _numerator (abs (n)), _denominator (1) {}

	T numerator() const { return _numerator; }
	T denominator() const { return _denominator; }

	bool is_unity() const { return _numerator == _denominator; }
	bool is_zero() const { return _numerator == 0; }

	/* provide an easy way to multiply double by ratio_t. Note that this
	   must be written as ratio_t * double, not the other way around. We
	   are not trying to duplicate boost::rational here (which also doesn't
	   allow this without a lot of syntactic fluff.
	*/
	double operator* (double v) const { return (v * (double) _numerator) / (double) _denominator; }

	/* ditto for int64_t */

	int64_t operator* (int64_t v) const { return int_div_round (v * _numerator, _denominator); }

                                        private:
	T _numerator;
	T _denominator;
};

typedef _ratio_t<int64_t> ratio_t;

enum OverlapType {
	OverlapNone,      // no overlap
	OverlapInternal,  // the overlap is 100% within the object
	OverlapStart,     // overlap covers start, but ends within
	OverlapEnd,       // overlap begins within and covers end
	OverlapExternal   // overlap extends to (at least) begin+end
};

enum TimeDomain {
	/* simple ordinals, since these are mutually exclusive */
	AudioTime = 0,
	BeatTime = 1,
	BarTime = 2,
};

enum Dirty {
	/* combinable */
	SampleDirty = 0x1,
	BeatsDirty = 0x2,
	BBTDirty = 0x4
};

enum RoundMode {
	RoundDownMaybe  = -2,  ///< Round down only if necessary
	RoundDownAlways = -1,  ///< Always round down, even if on a division
	RoundNearest    = 0,   ///< Round to nearest
	RoundUpAlways   = 1,   ///< Always round up, even if on a division
	RoundUpMaybe    = 2    ///< Round up only if necessary
};

extern void setup_enum_writer ();

}

#endif /* __libpbd_position_types_h__ */
