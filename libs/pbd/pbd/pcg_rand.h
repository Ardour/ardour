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

#include <assert.h>
#include <stdint.h>
#include <time.h>

namespace PBD {

/**
 * *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
 *
 * To be used in cases where an efficient and realtime-safe random
 * generator is needed.
 */
class PCGRand {
public:
  PCGRand ()
  {
    int foo = 0;
    uint64_t initseq = (intptr_t)&foo;
    _state = 0;
    _inc = (initseq << 1) | 1;
    rand_u32 ();
    _state += time (NULL) ^ (intptr_t)this;
    rand_u32 ();
  }

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

  /* uniform integer min <= rand <= max */
	int rand (int min, int max)
	{
		int range;
		if (min > max) {
			range = 1 + min - max;
			min = max;
		} else {
			range = 1 + max - min;
		}

		assert (range < 4294967296);

		while (true) {
			uint32_t value = rand_u32 ();
			if (value < 4294967296 - 4294967296 % range) {
				return min + value % range;
			}
		}
	}

  uint32_t rand_u32 ()
  {
    uint64_t oldstate = _state;
    _state = oldstate * 6364136223846793005ULL + _inc;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }

private:
  uint64_t _state;
  uint64_t _inc;
};

} // namespace PBD

#endif
