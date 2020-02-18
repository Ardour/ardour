/*
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <xmmintrin.h>
#include <immintrin.h>
#include <stdint.h>


void
x86_sse_avx_find_peaks(const float* buf, uint32_t nframes, float *min, float *max)
{
	__m256 current_max, current_min, work;

	// Load max and min values into all eight slots of the YMM registers
	current_min = _mm256_set1_ps(*min);
	current_max = _mm256_set1_ps(*max);

	// Work input until "buf" reaches 16 byte alignment
	while ( ((intptr_t)buf) % 32 != 0 && nframes > 0) {

		// Load the next float into the work buffer
		work = _mm256_set1_ps(*buf);

		current_min = _mm256_min_ps(current_min, work);
		current_max = _mm256_max_ps(current_max, work);

		buf++;
		nframes--;
	}

        // use 64 byte prefetch for quadruple quads:
		// load each 64 bytes into cash before processing
        while (nframes >= 16) {
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
				_mm_prefetch(((char*)buf+64), _mm_hint(0) );
#else
                __builtin_prefetch(buf+64,0,0);
#endif
                work = _mm256_load_ps(buf);
                current_min = _mm256_min_ps(current_min, work);
                current_max = _mm256_max_ps(current_max, work);
                buf+=8;
                work = _mm256_load_ps(buf);
                current_min = _mm256_min_ps(current_min, work);
                current_max = _mm256_max_ps(current_max, work);
                buf+=8;

                nframes-=16;
        }

	// work through 32 bytes aligned buffers
	while (nframes >= 8) {

		work = _mm256_load_ps(buf);

		current_min = _mm256_min_ps(current_min, work);
		current_max = _mm256_max_ps(current_max, work);

		buf+=8;
		nframes-=8;
	}

	// work through the rest < 4 samples
	while ( nframes > 0) {

		// Load the next float into the work buffer
		work = _mm256_set1_ps(*buf);

		current_min = _mm256_min_ps(current_min, work);
		current_max = _mm256_max_ps(current_max, work);

		buf++;
		nframes--;
	}

	// Find min & max value in current_max through shuffle tricks

	work = current_min;
	work =        _mm256_shuffle_ps (current_min, current_min, _MM_SHUFFLE(2, 3, 0, 1));
	current_min = _mm256_min_ps (work, current_min);
	work =        _mm256_shuffle_ps (current_min, current_min, _MM_SHUFFLE(1, 0, 3, 2));
	current_min = _mm256_min_ps (work, current_min);
	work =        _mm256_permute2f128_ps( current_min, current_min, 1);
	current_min = _mm256_min_ps (work, current_min);

	*min = current_min[0];

	work = current_max;
	work =        _mm256_shuffle_ps(current_max, current_max, _MM_SHUFFLE(2, 3, 0, 1));
	current_max = _mm256_max_ps (work, current_max);
	work =        _mm256_shuffle_ps(current_max, current_max, _MM_SHUFFLE(1, 0, 3, 2));
	current_max = _mm256_max_ps (work, current_max);
	work =        _mm256_permute2f128_ps( current_max, current_max, 1);
	current_max = _mm256_max_ps (work, current_max);

	*max = current_max[0];

	// zero upper 128 bit of 256 bit ymm register to avoid penalties using non-AVX instructions
	_mm256_zeroupper ();
}


