/*
    Copyright (C) 2012 Paul Davis 

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

/* Taken from
 * http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
 *
 * Code assumed to be in the public domain.
 */

#ifndef __libpbd__floating_h__
#define __libpbd__floating_h__

#include <stdint.h>

#include <cmath>

#include "pbd/libpbd_visibility.h"

namespace PBD {

union LIBPBD_API Float_t
{
    Float_t (float num = 0.0f) : f(num) {}

    // Portable extraction of components.
    bool    negative() const { return (i >> 31) != 0; }
    int32_t raw_mantissa() const { return i & ((1 << 23) - 1); }
    int32_t raw_exponent() const { return (i >> 23) & 0xFF; }
 
    int32_t i;
    float f;
};
 
/* Note: ULPS = Units in the Last Place */

static inline bool floateq (float a, float b, int max_ulps_diff)
{
    Float_t ua (a);
    Float_t ub (b);
 
    if (a == b) {
            return true;
    }

    // Different signs means they do not match.
    if (ua.negative() != ub.negative()) {
	    return false;
    }

    // Find the difference in ULPs.
    int ulps_diff = abs (ua.i - ub.i);

    if (ulps_diff <= max_ulps_diff) {
        return true;
    }
 
    return false;
}

} /* namespace */

#endif /* __libpbd__floating_h__ */
