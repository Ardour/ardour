/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <climits>
#include <ctime>

#include "pbd/pcg_rand.h"

using namespace PBD;

PCGRand::PCGRand ()
	: _state (0)
	, _foo (0)
{
	uint64_t initseq = (intptr_t)&_foo;
	_inc             = (initseq << 1) | 1;

	rand_u32 ();
	_state += time (NULL) ^ (intptr_t)this;
	rand_u32 ();
}

/* uniform integer min <= rand < max */
int
PCGRand::rand (int max, int min /* = 0 */)
{
	assert (min < max);
	assert (min > 0 || max < INT_MAX + min); // max - min overflow
	assert (min < 0 || max > INT_MIN + min); // max - min underflow

	const int range = max - min;

	while (true) {
		uint32_t value = rand_u32 ();
		if (value < 4294967295 - 4294967295 % range) {
			return min + value % range;
		}
	}
}

uint32_t
PCGRand::rand_u32 ()
{
	uint64_t oldstate   = _state;
	_state              = oldstate * 6364136223846793005ULL + _inc;
	uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32_t rot        = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}
