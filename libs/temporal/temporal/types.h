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

#include <stdint.h>

namespace Temporal {

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
