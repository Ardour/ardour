/*
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2020 Ayan Shafqat <ayan.x.shafqat@gmail.com>
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

#include "ardour/mix.h"

#include <immintrin.h>
#include <xmmintrin.h>

#ifndef __AVX__
#error "__AVX__ must be enabled for this module to work"
#endif

#define IS_ALIGNED_TO(ptr, bytes) (((uintptr_t)ptr) % (bytes) == 0)

#if defined(__GNUC__)
#define IS_NOT_ALIGNED_TO(ptr, bytes) \
	__builtin_expect(!!(reinterpret_cast<intptr_t>(ptr) % (bytes)), 0)
#else
#define IS_NOT_ALIGNED_TO(ptr, bytes) \
	(!!(reinterpret_cast<intptr_t>(ptr) % (bytes)))
#endif

#ifdef __cplusplus
#define C_FUNC extern "C"
#else
#define C_FUNC
#endif

/**
 * Local functions
 */

static inline __m256 avx_getmax_ps(__m256 vmax);
static inline __m256 avx_getmin_ps(__m256 vmin);

static void
x86_sse_avx_mix_buffers_with_gain_unaligned(float *dst, const float *src, uint32_t nframes, float gain);

static void
x86_sse_avx_mix_buffers_with_gain_aligned(float *dst, const float *src, uint32_t nframes, float gain);

static void
x86_sse_avx_mix_buffers_no_gain_unaligned(float *dst, const float *src, uint32_t nframes);

static void
x86_sse_avx_mix_buffers_no_gain_aligned(float *dst, const float *src, uint32_t nframes);

/**
 * Module implementation
 */

/**
 * @brief x86-64 AVX optimized routine for compute peak procedure
 * @param src Pointer to source buffer
 * @param nframes Number of frames to process
 * @param current Current peak value
 * @return float New peak value
 */
C_FUNC float
x86_sse_avx_compute_peak(const float *src, uint32_t nframes, float current)
{
	// If src is null then skip processing
	if ((src == nullptr) || (nframes == 0))
	{
		return current;
	}

	// Broadcast mask to compute absolute value
	const uint32_t f32_nan = UINT32_C(0x7FFFFFFF);
	const __m256 ABS_MASK =
		_mm256_broadcast_ss(reinterpret_cast<const float *>(&f32_nan));

	// Broadcast the current max value to all elements of the YMM register
	__m256 vmax = _mm256_set1_ps(current);

	// Compute single min/max of unaligned portion until alignment is reached
	while (IS_NOT_ALIGNED_TO(src, sizeof(__m256)) && (nframes > 0))
	{
		__m256 vsrc;

		vsrc = _mm256_broadcast_ss(src);
		vsrc = _mm256_and_ps(ABS_MASK, vsrc);
		vmax = _mm256_max_ps(vmax, vsrc);

		++src;
		--nframes;
	}

	// Process the aligned portion 32 samples at a time
	while (nframes >= 32)
	{
#ifdef _WIN32
		_mm_prefetch(reinterpret_cast<char const *>(src + 32), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 32), 0, 0);
#endif
		__m256 t0 = _mm256_load_ps(src + 0);
		__m256 t1 = _mm256_load_ps(src + 8);
		__m256 t2 = _mm256_load_ps(src + 16);
		__m256 t3 = _mm256_load_ps(src + 24);

		t0 = _mm256_and_ps(ABS_MASK, t0);
		t1 = _mm256_and_ps(ABS_MASK, t1);
		t2 = _mm256_and_ps(ABS_MASK, t2);
		t3 = _mm256_and_ps(ABS_MASK, t3);

		vmax = _mm256_max_ps(vmax, t0);
		vmax = _mm256_max_ps(vmax, t1);
		vmax = _mm256_max_ps(vmax, t2);
		vmax = _mm256_max_ps(vmax, t3);

		src += 32;
		nframes -= 32;
	}

	// Process the remaining samples 8 at a time
	while (nframes >= 8)
	{
		__m256 vsrc;

		vsrc = _mm256_load_ps(src);
		vsrc = _mm256_and_ps(ABS_MASK, vsrc);
		vmax = _mm256_max_ps(vmax, vsrc);

		src += 8;
		nframes -= 8;
	}

	// If there are still some left 4 to 8 samples, process them below
	while (nframes > 0)
	{
		__m256 vsrc;

		vsrc = _mm256_broadcast_ss(src);
		vsrc = _mm256_and_ps(ABS_MASK, vsrc);
		vmax = _mm256_max_ps(vmax, vsrc);

		++src;
		--nframes;
	}

	vmax = avx_getmax_ps(vmax);

#if defined(__GNUC__) && (__GNUC__ < 5)
	return *((float *)&vmax);
#elif defined(__GNUC__) && (__GNUC__ < 8)
	return vmax[0];
#else
	return _mm256_cvtss_f32(vmax);
#endif
}

/**
 * @brief x86-64 AVX optimized routine for find peak procedure
 * @param src Pointer to source buffer
 * @param nframes Number of frames to process
 * @param[in,out] minf Current minimum value, updated
 * @param[in,out] maxf Current maximum value, updated
 */
C_FUNC void
x86_sse_avx_find_peaks(const float *src, uint32_t nframes, float *minf, float *maxf)
{
	// Broadcast the current min and max values to all elements of the YMM register
	__m256 vmin = _mm256_broadcast_ss(minf);
	__m256 vmax = _mm256_broadcast_ss(maxf);

	// Compute single min/max of unaligned portion until alignment is reached
	while (IS_NOT_ALIGNED_TO(src, sizeof(__m256)) && nframes > 0) {
		__m256 vsrc;

		vsrc = _mm256_broadcast_ss(src);
		vmax = _mm256_max_ps(vmax, vsrc);
		vmin = _mm256_min_ps(vmin, vsrc);

		++src;
		--nframes;
	}

	// Process the aligned portion 32 samples at a time
	while (nframes >= 32)
	{
#ifdef _WIN32
		_mm_prefetch(reinterpret_cast<char const *>(src + 32), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 32), 0, 0);
#endif
		__m256 t0 = _mm256_load_ps(src + 0);
		__m256 t1 = _mm256_load_ps(src + 8);
		__m256 t2 = _mm256_load_ps(src + 16);
		__m256 t3 = _mm256_load_ps(src + 24);

		vmax = _mm256_max_ps(vmax, t0);
		vmax = _mm256_max_ps(vmax, t1);
		vmax = _mm256_max_ps(vmax, t2);
		vmax = _mm256_max_ps(vmax, t3);

		vmin = _mm256_min_ps(vmin, t0);
		vmin = _mm256_min_ps(vmin, t1);
		vmin = _mm256_min_ps(vmin, t2);
		vmin = _mm256_min_ps(vmin, t3);

		src += 32;
		nframes -= 32;
	}

	// Process the remaining samples 8 at a time
	while (nframes >= 8) {
		__m256 vsrc;

		vsrc = _mm256_load_ps(src);
		vmax = _mm256_max_ps(vmax, vsrc);
		vmin = _mm256_min_ps(vmin, vsrc);

		src += 8;
		nframes -= 8;
	}

	// If there are still some left 4 to 8 samples, process them one at a time.
	while (nframes > 0) {
		__m256 vsrc;

		vsrc = _mm256_broadcast_ss(src);
		vmax = _mm256_max_ps(vmax, vsrc);
		vmin = _mm256_min_ps(vmin, vsrc);

		++src;
		--nframes;
	}

	// Get min and max of the YMM registers
	vmin = avx_getmin_ps(vmin);
	vmax = avx_getmax_ps(vmax);

	_mm_store_ss(minf, _mm256_castps256_ps128(vmin));
	_mm_store_ss(maxf, _mm256_castps256_ps128(vmax));
}

/**
 * @brief x86-64 AVX optimized routine for apply gain routine
 * @param[in,out] dst Pointer to the destination buffer, which gets updated
 * @param nframes Number of frames (or samples) to process
 * @param gain Gain to apply
 */
C_FUNC void
x86_sse_avx_apply_gain_to_buffer(float *dst, uint32_t nframes, float gain)
{
	// Convert to signed integer to prevent any arithmetic overflow errors
	int32_t frames = (int32_t)nframes;
	// Load gain vector to all elements of YMM register
	__m256 vgain = _mm256_set1_ps(gain);

	do {
		__m128 g0 = _mm256_castps256_ps128(vgain);
		while (!IS_ALIGNED_TO(dst, sizeof(__m256)) && (frames > 0)) {
			_mm_store_ss(dst, _mm_mul_ps(g0, _mm_load_ss(dst)));
			++dst;
			--frames;
		}
	} while (0);

	// Process the remaining samples 16 at a time
	while (frames >= 16)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 16), 0, 0);
#endif
		__m256 d0, d1;
		d0 = _mm256_load_ps(dst + 0);
		d1 = _mm256_load_ps(dst + 8);

		d0 = _mm256_mul_ps(vgain, d0);
		d1 = _mm256_mul_ps(vgain, d1);

		_mm256_store_ps(dst + 0, d0);
		_mm256_store_ps(dst + 8, d1);

		dst += 16;
		frames -= 16;
	}

	// Process the remaining samples 8 at a time
	while (frames >= 8) {
		_mm256_store_ps(dst, _mm256_mul_ps(vgain, _mm256_load_ps(dst)));
		dst += 8;
		frames -= 8;
	}

	// Process the remaining samples
	do {
		__m128 g0 = _mm256_castps256_ps128(vgain);
		while (frames > 0) {
			_mm_store_ss(dst, _mm_mul_ps(g0, _mm_load_ss(dst)));
			++dst;
			--frames;
		}
	} while (0);
}

/**
 * @brief x86-64 AVX optimized routine for mixing buffer with gain.
 *
 * This function may choose SSE over AVX if the pointers are aligned
 * to 16 byte boundary instead of 32 byte boundary to reduce time to
 * process.
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 * @param gain Gain to apply
 */
C_FUNC void
x86_sse_avx_mix_buffers_with_gain(float *dst, const float *src, uint32_t nframes, float gain)
{
	if (IS_ALIGNED_TO(dst, 32) && IS_ALIGNED_TO(src, 32)) {
		// Pointers are both aligned to 32 bit boundaries, this can be processed with AVX
		x86_sse_avx_mix_buffers_with_gain_aligned(dst, src, nframes, gain);
	} else if (IS_ALIGNED_TO(dst, 16) && IS_ALIGNED_TO(src, 16)) {
		// This can still be processed with SSE
		x86_sse_mix_buffers_with_gain(dst, src, nframes, gain);
	} else {
		// Pointers are unaligned, so process them with unaligned load/store AVX
		x86_sse_avx_mix_buffers_with_gain_unaligned(dst, src, nframes, gain);
	}
}

/**
 * @brief x86-64 AVX optimized routine for mixing buffer with no gain.
 *
 * This function may choose SSE over AVX if the pointers are aligned
 * to 16 byte boundary instead of 32 byte boundary to reduce time to
 * process.
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 */
C_FUNC void
x86_sse_avx_mix_buffers_no_gain(float *dst, const float *src, uint32_t nframes)
{
	if (IS_ALIGNED_TO(dst, 32) && IS_ALIGNED_TO(src, 32)) {
		// Pointers are both aligned to 32 bit boundaries, this can be processed with AVX
		x86_sse_avx_mix_buffers_no_gain_aligned(dst, src, nframes);
	} else if (IS_ALIGNED_TO(dst, 16) && IS_ALIGNED_TO(src, 16)) {
		// This can still be processed with SSE
		x86_sse_mix_buffers_no_gain(dst, src, nframes);
	} else {
		// Pointers are unaligned, so process them with unaligned load/store AVX
		x86_sse_avx_mix_buffers_no_gain_unaligned(dst, src, nframes);
	}
}

/**
 * @brief Copy vector from one location to another
 *
 * This has not been hand optimized for AVX with the rationale that standard
 * C library implementation will provide faster memory copy operation. It will
 * be redundant to implement memcpy for floats.
 *
 * @param[out] dst Pointer to destination buffer
 * @param[in] src Pointer to source buffer
 * @param nframes Number of samples to copy
 */
C_FUNC void
x86_sse_avx_copy_vector(float *dst, const float *src, uint32_t nframes)
{
	(void) memcpy(dst, src, nframes * sizeof(float));
}

/**
 * Local helper functions
 */

/**
 * @brief Helper routine for mixing buffers with gain for unaligned buffers
 *
 * @details This routine executes the following expression below per element:
 *
 * dst = dst + (gain * src)
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 * @param gain Gain to apply
 */
static void
x86_sse_avx_mix_buffers_with_gain_unaligned(float *dst, const float *src, uint32_t nframes, float gain)
{
	// Load gain vector to all elements of YMM register
	__m256 vgain = _mm256_set1_ps(gain);

	// Process the remaining samples 16 at a time
	while (nframes >= 16)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
		_mm_prefetch(((char *)src + (16 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 16), 0, 0);
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 16), 0, 0);
#endif
		__m256 s0, s1;
		__m256 d0, d1;

		// Load sources
		s0 = _mm256_loadu_ps(src + 0);
		s1 = _mm256_loadu_ps(src + 8);

		// Load destinations
		d0 = _mm256_loadu_ps(dst + 0);
		d1 = _mm256_loadu_ps(dst + 8);

		// src = src * gain
		s0 = _mm256_mul_ps(vgain, s0);
		s1 = _mm256_mul_ps(vgain, s1);

		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		d1 = _mm256_add_ps(d1, s1);

		// Store result
		_mm256_storeu_ps(dst + 0, d0);
		_mm256_storeu_ps(dst + 8, d1);

		// Update pointers and counters
		src += 16;
		dst += 16;
		nframes -= 16;
	}

	// Process the remaining samples 8 at a time
	while (nframes >= 8) {
		__m256 s0, d0;
		// Load sources
		s0 = _mm256_loadu_ps(src);
		// Load destinations
		d0 = _mm256_loadu_ps(dst);
		// src = src * gain
		s0 = _mm256_mul_ps(vgain, s0);
		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		// Store result
		_mm256_storeu_ps(dst, d0);
		// Update pointers and counters
		src+= 8;
		dst += 8;
		nframes -= 8;
	}

	// Process the remaining samples
	do {
		__m128 g0 = _mm_set_ss(gain);
		while (nframes > 0) {
			__m128 s0, d0;
			s0 = _mm_load_ss(src);
			d0 = _mm_load_ss(dst);
			s0 = _mm_mul_ss(g0, s0);
			d0 = _mm_add_ss(d0, s0);
			_mm_store_ss(dst, d0);
			++src;
			++dst;
			--nframes;
		}
	} while (0);
}

/**
 * @brief Helper routine for mixing buffers with gain for aligned buffers
 *
 * @details This routine executes the following expression below per element:
 *
 * dst = dst + (gain * src)
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 * @param gain Gain to apply
 */
static void
x86_sse_avx_mix_buffers_with_gain_aligned(float *dst, const float *src, uint32_t nframes, float gain)
{
	// Load gain vector to all elements of YMM register
	__m256 vgain = _mm256_set1_ps(gain);

	// Process the remaining samples 16 at a time
	while (nframes >= 16)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
		_mm_prefetch(((char *)src + (16 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 16), 0, 0);
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 16), 0, 0);
#endif
		__m256 s0, s1;
		__m256 d0, d1;

		// Load sources
		s0 = _mm256_load_ps(src + 0);
		s1 = _mm256_load_ps(src + 8);

		// Load destinations
		d0 = _mm256_load_ps(dst + 0);
		d1 = _mm256_load_ps(dst + 8);

		// src = src * gain
		s0 = _mm256_mul_ps(vgain, s0);
		s1 = _mm256_mul_ps(vgain, s1);

		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		d1 = _mm256_add_ps(d1, s1);

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
		// src = src * gain
		s0 = _mm256_mul_ps(vgain, s0);
		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		// Store result
		_mm256_store_ps(dst, d0);
		// Update pointers and counters
		src += 8;
		dst += 8;
		nframes -= 8;
	}

	// Process the remaining samples, one sample at a time.
	do {
		__m128 g0 = _mm256_castps256_ps128(vgain); // use the same register
		while (nframes > 0) {
			__m128 s0, d0;
			s0 = _mm_load_ss(src);
			d0 = _mm_load_ss(dst);
			s0 = _mm_mul_ss(g0, s0);
			d0 = _mm_add_ss(d0, s0);
			_mm_store_ss(dst, d0);
			++src;
			++dst;
			--nframes;
		}
	} while (0);
}

/**
 * @brief Helper routine for mixing buffers with no gain for aligned buffers
 *
 * @details This routine executes the following expression below per element:
 *
 * dst = dst + src
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 */
static void
x86_sse_avx_mix_buffers_no_gain_unaligned(float *dst, const float *src, uint32_t nframes)
{
	// Process the remaining samples 16 at a time
	while (nframes >= 16)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
		_mm_prefetch(((char *)src + (16 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 16), 0, 0);
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 16), 0, 0);
#endif
		__m256 s0, s1;
		__m256 d0, d1;

		// Load sources
		s0 = _mm256_loadu_ps(src + 0);
		s1 = _mm256_loadu_ps(src + 8);

		// Load destinations
		d0 = _mm256_loadu_ps(dst + 0);
		d1 = _mm256_loadu_ps(dst + 8);

		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		d1 = _mm256_add_ps(d1, s1);

		// Store result
		_mm256_storeu_ps(dst + 0, d0);
		_mm256_storeu_ps(dst + 8, d1);

		// Update pointers and counters
		src += 16;
		dst += 16;
		nframes -= 16;
	}

	// Process the remaining samples 8 at a time
	while (nframes >= 8) {
		__m256 s0, d0;
		// Load sources
		s0 = _mm256_loadu_ps(src);
		// Load destinations
		d0 = _mm256_loadu_ps(dst);
		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		// Store result
		_mm256_storeu_ps(dst, d0);
		// Update pointers and counters
		src+= 8;
		dst += 8;
		nframes -= 8;
	}

	// Process the remaining samples
	do {
		while (nframes > 0) {
			__m128 s0, d0;
			s0 = _mm_load_ss(src);
			d0 = _mm_load_ss(dst);
			d0 = _mm_add_ss(d0, s0);
			_mm_store_ss(dst, d0);
			++src;
			++dst;
			--nframes;
		}
	} while (0);

}

/**
 * @brief Helper routine for mixing buffers with no gain for unaligned buffers
 *
 * @details This routine executes the following expression below per element:
 *
 * dst = dst + src
 *
 * @param[in,out] dst Pointer to destination buffer, which gets updated
 * @param[in] src Pointer to source buffer (not updated)
 * @param nframes Number of samples to process
 */
static void
x86_sse_avx_mix_buffers_no_gain_aligned(float *dst, const float *src, uint32_t nframes)
{
	// Process the aligned portion 32 samples at a time
	while (nframes >= 32)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (32 * sizeof(float))), _mm_hint(0));
		_mm_prefetch(((char *)src + (32 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 32), 0, 0);
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 32), 0, 0);
#endif
		__m256 s0, s1, s2, s3;
		__m256 d0, d1, d2, d3;

		// Load sources
		s0 = _mm256_load_ps(src + 0 );
		s1 = _mm256_load_ps(src + 8 );
		s2 = _mm256_load_ps(src + 16);
		s3 = _mm256_load_ps(src + 24);

		// Load destinations
		d0 = _mm256_load_ps(dst + 0 );
		d1 = _mm256_load_ps(dst + 8 );
		d2 = _mm256_load_ps(dst + 16);
		d3 = _mm256_load_ps(dst + 24);

		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		d1 = _mm256_add_ps(d1, s1);
		d2 = _mm256_add_ps(d2, s2);
		d3 = _mm256_add_ps(d3, s3);

		// Store result
		_mm256_store_ps(dst + 0 , d0);
		_mm256_store_ps(dst + 8 , d1);
		_mm256_store_ps(dst + 16, d2);
		_mm256_store_ps(dst + 24, d3);

		// Update pointers and counters
		src += 32;
		dst += 32;
		nframes -= 32;
	}

	// Process the remaining samples 16 at a time
	while (nframes >= 16)
	{
#if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
		_mm_prefetch(((char *)dst + (16 * sizeof(float))), _mm_hint(0));
		_mm_prefetch(((char *)src + (16 * sizeof(float))), _mm_hint(0));
#else
		__builtin_prefetch(reinterpret_cast<void const *>(src + 16), 0, 0);
		__builtin_prefetch(reinterpret_cast<void const *>(dst + 16), 0, 0);
#endif
		__m256 s0, s1;
		__m256 d0, d1;

		// Load sources
		s0 = _mm256_load_ps(src + 0);
		s1 = _mm256_load_ps(src + 8);

		// Load destinations
		d0 = _mm256_load_ps(dst + 0);
		d1 = _mm256_load_ps(dst + 8);

		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		d1 = _mm256_add_ps(d1, s1);

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
		// dst = dst + src
		d0 = _mm256_add_ps(d0, s0);
		// Store result
		_mm256_store_ps(dst, d0);
		// Update pointers and counters
		src += 8;
		dst += 8;
		nframes -= 8;
	}

	// Process the remaining samples
	do {
		while (nframes > 0) {
			__m128 s0, d0;
			s0 = _mm_load_ss(src);
			d0 = _mm_load_ss(dst);
			d0 = _mm_add_ss(d0, s0);
			_mm_store_ss(dst, d0);
			++src;
			++dst;
			--nframes;
		}
	} while (0);
}

/**
 * @brief Get the maximum value of packed float register
 * @param vmax Packed float 8x register
 * @return __m256 Maximum value in p[0]
 */
static inline __m256 avx_getmax_ps(__m256 vmax)
{
	vmax = _mm256_max_ps(vmax, _mm256_permute2f128_ps(vmax, vmax, 1));
	vmax = _mm256_max_ps(vmax, _mm256_permute_ps(vmax, _MM_SHUFFLE(0, 0, 3, 2)));
	vmax = _mm256_max_ps(vmax, _mm256_permute_ps(vmax, _MM_SHUFFLE(0, 0, 0, 1)));
	return vmax;
}

/**
 * @brief Get the minimum value of packed float register
 * @param vmax Packed float 8x register
 * @return __m256 Minimum value in p[0]
 */
static inline __m256 avx_getmin_ps(__m256 vmin)
{
	vmin = _mm256_min_ps(vmin, _mm256_permute2f128_ps(vmin, vmin, 1));
	vmin = _mm256_min_ps(vmin, _mm256_permute_ps(vmin, _MM_SHUFFLE(0, 0, 3, 2)));
	vmin = _mm256_min_ps(vmin, _mm256_permute_ps(vmin, _MM_SHUFFLE(0, 0, 0, 1)));
	return vmin;
}
