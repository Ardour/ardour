/* This file is part of Evoral.
 * Copyright (C) 2008-2015 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef TEMPORAL_MATH_UTILS_HPP
#define TEMPORAL_MATH_UTILS_HPP

namespace Temporal {
	/**
	 * Integer division that rounds toward negative infinity instead of toward
	 * zero. Returns the quotient, and stores the remainder in *remainder if
	 * remainder is non-null. The remainder is always nonnegative.
	 */
	template <typename Number> static Number div_rtni(Number numer,
	                                                  Number denom,
	                                                  Number* remainder)
	{
		Number result_quotient = numer / denom;
		Number result_remainder = numer % denom;

		if (result_remainder < 0) {
			result_remainder += denom;
			--result_quotient;
		}

		if (remainder) {
			*remainder = result_remainder;
		}

		return result_quotient;
	}
} // namespace Temporal

#endif // TEMPORAL_MATH_UTILS_HPP
