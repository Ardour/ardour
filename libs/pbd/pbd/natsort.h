/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef PBD_NATSORT
#define PBD_NATSORT

#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>

namespace PBD {

inline bool
is_integer (const char* i)
{
	return isdigit (*i) || (*i == '-' && isdigit (i[1]));
}

/* return scale factor for SI metric prefix x 1000
 * (to allow milli for integers)
 */
inline int64_t
order_of_magnitude (const char* i)
{
	if (!is_integer (i)) {
		return 0;
	}
	while (isdigit (*++i)) ;
	if (!*i) {
		return 1e3;
	}
	switch (*i) {
		case 'm':
			return 1;
		case 'c':
			return 10;
		case 'd':
			return 100;
		case 'k':
			/* fallthrough */
		case 'K':
			return 1e6;
		case 'M':
			return 1e9;
		case 'G':
			return 1e12;
		case 'T':
			return 1e15;
	}
	return 1e3;
}

/* this method sorts negative integers before
 * positive ones, and also handles hexadecimal
 * numbers when prefixed with "0x" or "0X".
 * SI metric prefixes for integers are handled.
 * (floating point, and rational numbers are
 *  not directy handled)
 */
inline bool
numerically_less (const char* a, const char* b)
{
	const char* d_a = NULL;
	const char* d_b = NULL;

	for (;*a && *b; ++a, ++b) {
		if (is_integer (a) && is_integer (b) && !d_a) {
			d_a = a; d_b = b;
			continue;
		}
		if (d_a) {
			/* strip leading zeros to prevent `strtol` from using octal */
			while (*d_a == '0') { if (d_a[1] && isdigit (d_a[1])) { ++d_a; } else { break; } }
			while (*d_b == '0') { if (d_b[1] && isdigit (d_b[1])) { ++d_b; } else { break; } }

			const int64_t ia = strtol (d_a, NULL, 0) * order_of_magnitude (d_a);
			const int64_t ib = strtol (d_b, NULL, 0) * order_of_magnitude (d_b);
			if (ia != ib) {
				return ia < ib;
			}
		}
		d_a = d_b = NULL;
		if (*a == *b) {
			continue;
		}
		return *a < *b;
	}

	if (d_a) {
		return strtol (d_a, NULL, 0) * order_of_magnitude (d_a) < strtol (d_b, NULL, 0) * order_of_magnitude (d_b);
	}

	/* if we reach here, either strings are same length and equal
	 * or one is longer than the other.
	 */

	if (*a) { return false; }
	if (*b) { return true; }
	return false; // equal
}

inline int
natcmp (const char* a, const char* b)
{
	const char* d_a = NULL;
	const char* d_b = NULL;

	for (;*a && *b; ++a, ++b) {
		if (isdigit (*a) && isdigit (*b) && !d_a) {
			d_a = a; d_b = b;
			continue;
		}
		if (d_a) {
			const int ia = atoi (d_a);
			const int ib = atoi (d_b);
			if (ia != ib) {
				return ia < ib ? -1 : 1;
			}
		}
		d_a = d_b = NULL;
		if (*a == *b) {
			continue;
		}
#if 1
		/* treat underscore as space, this works around idiosyncratic
		 * ffado port-names: "foo_in", "foo0_in", "foo2_in", etc */
		if (*a == '_' && *b == ' ') {
			continue;
		}
		if (*b == '_' && *a == ' ') {
			continue;
		}
		if (*a == '_') {
			return ' ' < *b ? -1 : 1;
		} else if (*b == '_') {
			return *a < ' ' ? -1 : 1;
		} else
#endif
		return *a < *b ?  -1 : 1;
	}

	if (d_a) {
		const int ia = atoi (d_a);
		const int ib = atoi (d_b);
		if (ia != ib) {
			return ia < ib ? -1 : 1;
		}
	}

	/* if we reach here, either strings are same length and equal
	 * or one is longer than the other.
	 */
	if (*a) { return 1; }
	if (*b) { return -1; }
	return 0;
}

inline bool
naturally_less (const char* a, const char* b)
{
	return natcmp (a, b) < 0;
}

} // namespace PBD

#endif // PBD_NATSORT
