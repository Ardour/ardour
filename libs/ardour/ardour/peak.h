/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_peak_h__
#define __ardour_peak_h__

#include <cmath>
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/utils.h"

static inline float
default_compute_peak (const ARDOUR::Sample * const buf, ARDOUR::pframes_t nsamples, float current)
{
	for (ARDOUR::pframes_t i = 0; i < nsamples; ++i) {
		current = f_max (current, fabsf (buf[i]));
	}
	return current;
}

#endif /* __ardour_peak_h__ */
