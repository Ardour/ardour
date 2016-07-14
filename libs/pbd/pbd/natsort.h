/*
 * Copyright 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

namespace PBD {

inline bool
naturally_less (const char* a, const char* b)
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
		return atoi (d_a) < atoi (d_b);
	}

	/* if we reach here, either strings are same length and equal
	 * or one is longer than the other.
	 */

	if (*a) { return false; }
	if (*b) { return true; }
	return false; // equal
}

} // namespace PBD

#endif // PBD_NATSORT
