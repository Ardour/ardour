/*
    Copyright (C) 2020 Paul Davis

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

#ifndef __libpbd_integer_division_h__
#define __libpbd_integer_division_h__

#include <cstdint>

#ifndef COMPILER_INT128_SUPPORT
#include <boost/multiprecision/cpp_int.hpp>
#include "pbd/error.h"
#endif

#define PBD_IDIV_ASR(x) ((x) < 0 ? -1 : 0)  // Compiles into a (N-1)-bit arithmetic shift right

/* The value of PBD_IDIV_ROUNDING will have the same sign as the dividend (x) and half
 * the magnitude of the divisor (y). Adding ROUNDING to the dividend thus
 * increases its magnitude before the integer division truncates the resulting
 * quotient.
 */

#define PBD_IDIV_ROUNDING(x,y) ( (y)/2 - (PBD_IDIV_ASR((x)^(y)) & (y)))

template<typename T>
T int_div_round (T x, T y)
{
	/* essentially ((x + (y/2)) / y) but handles signed/negative values correcvtly */
	return (x + PBD_IDIV_ROUNDING(x,y)) / y ;
}

namespace PBD {

/* this computes v * (n/d) where v, n and d are all 64 bit integers, without
 * overflow, and with appropriate rounding given that this is integer division.
 */

inline
int64_t muldiv (int64_t v, int64_t n, int64_t d)
{
	/* either n or d or both could be negative but for now we assume that
	   only d could be (that is, n and d represent negative rational numbers of the
	   form 1/-2 rather than -1/2). This follows the behavior of the
	   ratio_t type in the temporal library.

	   Consequently, we only use d in the rounding-signdness expression.
	*/
	const int64_t hd = PBD_IDIV_ROUNDING (v, d);

#ifndef COMPILER_INT128_SUPPORT
	boost::multiprecision::int512_t bignum = v;

	bignum *= n;
	bignum += hd;
	bignum /= d;

	try {

		return bignum.convert_to<int64_t> ();

	} catch (...) {
		fatal << "arithmetic overflow in timeline math\n" << endmsg;
		/* NOTREACHED */
		return 0;
	}

#else
	__int128 _n (n);
	__int128 _d (d);
	__int128 _v (v);

	/* this could overflow, but will not do so merely because we are
	 * multiplying two int64_t together and storing the result in an
	 * int64_t. Overflow will occur where (v*n)+hd > INT128_MAX (hard
	 * limit) or where v * n / d > INT64_T (i.e. n > d)
	 */

	return(int64_t) (((_v * _n) + hd) / _d);
#endif
}
} /* namespace */

#endif /* __libpbd_integer_division_h___ */
