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

#ifndef __ardour_superclock_h__
#define __ardour_superclock_h__

#include <stdint.h>

#include "pbd/integer_division.h"

#include "temporal/visibility.h"

namespace Temporal {

typedef int64_t superclock_t;

#ifndef COMPILER_MSVC
	extern superclock_t _superclock_ticks_per_second;
#else
	static superclock_t _superclock_ticks_per_second = 56448000; /* 2^10 * 3^2 * 5^3 * 7^2 */
#endif

extern bool scts_set;

#ifdef DEBUG_EARLY_SCTS_USE

#include <cstdlib>
#include <csignal>

static inline superclock_t superclock_ticks_per_second() { if (!scts_set) { raise (SIGUSR2); } return _superclock_ticks_per_second; }
#else
static inline superclock_t superclock_ticks_per_second() { return _superclock_ticks_per_second; }
#endif

static inline superclock_t superclock_to_samples (superclock_t s, int sr) { return int_div_round (s * sr, superclock_ticks_per_second()); }
static inline superclock_t samples_to_superclock (int64_t samples, int sr) { return int_div_round (samples * superclock_ticks_per_second(), superclock_t (sr)); }

extern int most_recent_engine_sample_rate;

LIBTEMPORAL_API void set_sample_rate (int sr);
LIBTEMPORAL_API void set_superclock_ticks_per_second (superclock_t sc);

}

#define TEMPORAL_SAMPLE_RATE (Temporal::most_recent_engine_sample_rate)

#endif /* __ardour_superclock_h__ */
