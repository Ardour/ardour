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

#endif /* __libpbd_integer_division_h___ */
