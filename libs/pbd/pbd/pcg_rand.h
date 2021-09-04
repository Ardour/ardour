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

#ifndef _pbd_pcg_rand_
#define _pbd_pcg_rand_

#include <stdint.h>
#include "pbd/libpbd_visibility.h"

namespace PBD
{
/**
 * *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
 *
 * To be used in cases where an efficient and realtime-safe random
 * generator is needed.
 */
class LIBPBD_API PCGRand
{
public:
	PCGRand ();

	/* 32bit (0 .. 4294967295) */
	uint32_t rand_u32 ();

	/* uniform integer min <= rand < max */
	int rand (int max, int min = 0);

	/* unsigned float [0..1] */
	float rand_uf ()
	{
		return rand_u32 () / 4294967295.f;
	}

	/* signed float [-1..+1] */
	float rand_sf ()
	{
		return (rand_u32 () / 2147483647.5f) - 1.f;
	}

private:
	uint64_t _state;
	uint64_t _inc;
	int      _foo;
};

} // namespace PBD

#endif
