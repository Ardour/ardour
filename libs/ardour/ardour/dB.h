/*
    Copyright (C) 2001 Paul Davis

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

#ifndef __ardour_dB_h__
#define __ardour_dB_h__

#include "pbd/fastlog.h"

#define GAIN_COEFF_ZERO 0.0
#define GAIN_COEFF_SMALL 0.0000001  //-140dB
#define GAIN_COEFF_UNITY 1.0

static inline float dB_to_coefficient (float dB) {
	return dB > -318.8f ? pow (10.0f, dB * 0.05f) : 0.0f;
}

static inline float fast_coefficient_to_dB (float coeff) {
	return 20.0f * fast_log10 (coeff);
}

static inline float accurate_coefficient_to_dB (float coeff) {
	return 20.0f * log10f (coeff);
}

static inline double dB_coeff_step(double max_coeff) {
	const double max_db = lrint(accurate_coefficient_to_dB(max_coeff));
	return 0.1 * (max_coeff / max_db);
}

extern double zero_db_as_fraction;

#endif /* __ardour_dB_h__ */
