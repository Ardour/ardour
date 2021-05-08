/*
 * Copyright (C) 2020 Ayan Shafqat <ayan.x.shafqat@gmail.com>
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

#ifdef FPU_AVX_FMA_SUPPORT

#include "ardour/mix.h"

#include <immintrin.h>
#include <xmmintrin.h>

#define IS_ALIGNED_TO(ptr, bytes) (((uintptr_t)ptr) % (bytes) == 0)

/**
 * @brief x86-64 AVX/FMA optimized routine for mixing buffer with gain.
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 * @param gain Gain to apply
 */
void
x86_fma_mix_buffers_with_gain(
    float       *dst,
    const float *src,
    uint32_t     nframes,
    float gain)
{
	// While buffers aren't aligned, then process one sample at a time
	do {
		__m128 g0 = _mm_set_ss(gain); // Should be a no-op

		while (!(IS_ALIGNED_TO(src, sizeof(__m256)) &&
				IS_ALIGNED_TO(dst, sizeof(__m256))) &&
				(nframes > 0)) {

			__m128 x0 = _mm_load_ss(src);
			__m128 y0 = _mm_load_ss(dst);
			__m128 z0 = _mm_fmadd_ss(x0, g0, y0);
			_mm_store_ss(dst, z0);

			++dst;
			++src;
			--nframes;
		}
	} while (0);


	// Use AVX registers to process 16 samples in parallel
	do {
		__m256 g0 = _mm256_set1_ps(gain);

		while (nframes >= 16) {
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
			_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
			_mm_prefetch(((char *)src + (16 * sizeof(float))), _mm_hint(0));
#else
			__builtin_prefetch(src + (16 * sizeof(float)), 0, 0);
			__builtin_prefetch(dst + (16 * sizeof(float)), 0, 0);
#endif
			__m256 s0, s1;
			__m256 d0, d1;

			// Load sources
			s0 = _mm256_load_ps(src + 0);
			s1 = _mm256_load_ps(src + 8);

			// Load destinations
			d0 = _mm256_load_ps(dst + 0);
			d1 = _mm256_load_ps(dst + 8);

			// dst = dst + (src * gain)
			d0 = _mm256_fmadd_ps(g0, s0, d0);
			d1 = _mm256_fmadd_ps(g0, s1, d1);

			// Store result
			_mm256_store_ps(dst + 0, d0);
			_mm256_store_ps(dst + 8, d1);

			// Update pointers and counters
			src += 16;
			dst += 16;
			nframes -= 16;
		}

		// Process the remaining samples 8 at a time
		while (nframes >= 8) {
			__m256 s0, d0;
			// Load sources
			s0 = _mm256_load_ps(src + 0 );
			// Load destinations
			d0 = _mm256_load_ps(dst + 0 );
			// dst = dst + (src * gain)
			d0 = _mm256_fmadd_ps(g0, s0, d0);
			// Store result
			_mm256_store_ps(dst, d0);
			// Update pointers and counters
			src += 8;
			dst += 8;
			nframes -= 8;
		}
	} while (0);

	// There's a penalty going from AVX mode to SSE mode. This can
	// be avoided by ensuring the CPU that rest of the routine is no
	// longer interested in the upper portion of the YMM register.

	_mm256_zeroupper(); // zeros the upper portion of YMM register

	// Process the remaining samples, one sample at a time.
	do {
		__m128 g0 = _mm_set_ss(gain); // Should be a no-op

		while (nframes > 0) {
			__m128 x0 = _mm_load_ss(src);
			__m128 y0 = _mm_load_ss(dst);
			__m128 z0 = _mm_fmadd_ss(x0, g0, y0);
			_mm_store_ss(dst, z0);
			++dst;
			++src;
			--nframes;
		}
	} while (0);
}

#endif // FPU_AVX_FMA_SUPPORT
