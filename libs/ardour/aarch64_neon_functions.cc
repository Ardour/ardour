/*
 * Copyright (C) 2025 Ayan Shafqat <ayan.x.shafqat@gmail.com>
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

#ifdef ARM_NEON_SUPPORT

#include <arm_acle.h>
#include <arm_neon.h>

#include <cstddef>
#include <algorithm>

/**
 * @brief Aligns a pointer to the next 16-byte boundary
 *
 * @param ptr Pointer to be aligned
 * @return void* Aligned pointer
 */
#define ALIGN_PTR_NEXT_16(ptr) ((void*) ((((uintptr_t) ptr) + 0xF) & ~0xf))

/**
 * @brief GCC builtin to hint the compiler that the expression is unlikely
 */
#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif

#ifdef __cplusplus
#define C_FUNC extern "C"
#else
#define C_FUNC
#endif

/**
 * @brief Compute the absolute peak value in a buffer of floats
 *
 * This function computes the peak value in a buffer of floats. The function
 * uses NEON SIMD instructions to compute the peak value, with some loop
 * unrolling. The function is optimized for both Aarch64 and Aarch32 platforms.
 *
 * The performance of this function does not depend on the alignment of the
 * input buffer, as the function handles misaligned samples before the first
 * aligned address. However, it will be performant if the buffer is aligned to
 * a 16-byte boundary, and the number of frames is a multiple of 16.
 *
 * @param[in] src Pointer to the buffer of floats
 * @param[in] nframes Number of frames in the buffer
 * @param[in] current Current peak value
 * @return float
 */
C_FUNC float
arm_neon_compute_peak(const float* src, uint32_t nframes, float current)
{
	// Broadcast single value to all elements of the register
	float32x4_t vmax = vdupq_n_f32(current);

	// Compute the next aligned pointer
	const float* src_aligned = (const float*) ALIGN_PTR_NEXT_16(src);

	// Process misaligned samples before the first aligned address
	if (UNLIKELY(src_aligned != src))
	{
		size_t unaligned_count = src_aligned - src;

		// Handle small number of nframes
		size_t count = std::min<size_t>(unaligned_count, nframes);

		for (size_t i = 0; i < count; i++)
		{
			float32x4_t x0 = vld1q_dup_f32(src + i);
			x0 = vabsq_f32(x0);
			vmax = vmaxq_f32(vmax, x0);
		}

		nframes -= count;
	}

	// Compute the number of SIMD frames
	size_t simd_count = nframes / 4;
	size_t nframes_simd = 4 * simd_count;
	size_t start = 0;

	// Some loop unrolling
	if (simd_count >= 4)
	{
		size_t unrolled_count = simd_count / 4;
		for (size_t i = start; i < unrolled_count; ++i)
		{
			size_t offset = 4 * 4 * i;
			float32x4_t x0, x1, x2, x3;
			float32x4_t max0, max1, max2;

			x0 = vld1q_f32(src_aligned + offset + (0 * 4));
			x1 = vld1q_f32(src_aligned + offset + (1 * 4));
			x2 = vld1q_f32(src_aligned + offset + (2 * 4));
			x3 = vld1q_f32(src_aligned + offset + (3 * 4));

			x0 = vabsq_f32(x0);
			x1 = vabsq_f32(x1);
			x2 = vabsq_f32(x2);
			x3 = vabsq_f32(x3);

			max0 = vmaxq_f32(x0, x1);
			max1 = vmaxq_f32(x2, x3);
			max2 = vmaxq_f32(max0, max1);
			vmax = vmaxq_f32(vmax, max2);
		}

		start = unrolled_count * 4;
	}

	for (size_t i = start; i < simd_count; ++i)
	{
		size_t offset = 4 * i;
		float32x4_t x0;
		x0 = vld1q_f32(src_aligned + offset);
		vmax = vmaxq_f32(vmax, x0);
	}

	for (size_t frame = nframes_simd; frame < nframes; ++frame)
	{
		float32x4_t x0;
		x0 = vld1q_dup_f32(src_aligned + frame);
		vmax = vmaxq_f32(vmax, x0);
	}

	// Compute the max in register
#if (__aarch64__ == 1)
	{
		current = vmaxvq_f32(vmax);
	}
#else
	{
		float32x2_t vlo = vget_low_f32(vmax);
		float32x2_t vhi = vget_high_f32(vmax);
		float32x2_t max0 = vpmax_f32(vlo, vhi);
		float32x2_t max1 = vpmax_f32(max0, max0); // Max is now at max1[0]
		current = vget_lane_f32(max1, 0);
	}
#endif

	return current;
}

/**
 * @brief Find the minimum and maximum values in a buffer of floats
 *
 * This function computes the minimum and maximum values in a buffer of floats.
 * The function uses NEON SIMD instructions to compute the min and max values,
 * with some loop unrolling. The function is optimized for both Aarch64 and
 * Aarch32 platforms.
 *
 * Similar to @p arm_neon_compute_peak, the performance of this function does
 * not depend on the alignment of the input buffer, as the function handles
 * misaligned samples before the first aligned address. However, it will be
 * performant if the buffer is aligned to a 16-byte boundary, and the number of
 * frames is a multiple of 16.
 *
 * @param[in] src Pointer to the buffer of floats
 * @param[in] nframes Number of frames in the buffer
 * @param[in,out] minf Pointer to the minimum value (gets updated)
 * @param[in,out] maxf Pointer to the maximum value (gets updated)
 */
C_FUNC void
arm_neon_find_peaks(const float* src, uint32_t nframes, float* minf, float* maxf)
{
	float32x4_t vmin, vmax;

	// Broadcast single value to all elements of the register
	vmin = vld1q_dup_f32(minf);
	vmax = vld1q_dup_f32(maxf);

	const float* src_aligned = (const float*) ALIGN_PTR_NEXT_16(src);

	// Process misaligned samples before the first aligned address
	if (UNLIKELY(src_aligned != src))
	{
		size_t unaligned_count = src_aligned - src;

		// Handle small number of nframes
		size_t count = std::min<size_t>(unaligned_count, nframes);

		for (size_t i = 0; i < count; i++)
		{
			float32x4_t x0 = vld1q_dup_f32(src + i);
			vmax = vmaxq_f32(vmax, x0);
			vmin = vminq_f32(vmin, x0);
		}

		nframes -= count;
	}

	// Compute the number of SIMD frames
	size_t simd_count = nframes / 4;
	size_t nframes_simd = 4 * simd_count;
	size_t start = 0;

	// Some loop unrolling
	if (simd_count >= 4)
	{
		size_t unrolled_count = simd_count / 4;
		for (size_t i = start; i < unrolled_count; ++i)
		{
			size_t offset = 4 * 4 * i;
			float32x4_t x0, x1, x2, x3;
			float32x4_t max0, max1, max2;
			float32x4_t min0, min1, min2;

			x0 = vld1q_f32(src_aligned + offset + (0 * 4));
			x1 = vld1q_f32(src_aligned + offset + (1 * 4));
			x2 = vld1q_f32(src_aligned + offset + (2 * 4));
			x3 = vld1q_f32(src_aligned + offset + (3 * 4));

			max0 = vmaxq_f32(x0, x1);
			max1 = vmaxq_f32(x2, x3);
			max2 = vmaxq_f32(max0, max1);
			vmax = vmaxq_f32(vmax, max2);

			min0 = vminq_f32(x0, x1);
			min1 = vminq_f32(x2, x3);
			min2 = vminq_f32(min0, min1);
			vmin = vminq_f32(vmin, min2);
		}

		start = unrolled_count * 4;
	}

	for (size_t i = start; i < simd_count; ++i)
	{
		size_t offset = 4 * i;
		float32x4_t x0;
		x0 = vld1q_f32(src_aligned + offset);
		vmax = vmaxq_f32(vmax, x0);
		vmin = vminq_f32(vmin, x0);
	}

	for (size_t frame = nframes_simd; frame < nframes; ++frame)
	{
		float32x4_t x0;
		x0 = vld1q_dup_f32(src_aligned + frame);
		vmax = vmaxq_f32(vmax, x0);
		vmin = vminq_f32(vmin, x0);
	}

	// Compute the max in register
#if (__aarch64__ == 1)
	{
		float max_val = vmaxvq_f32(vmax);
		*maxf = max_val;
	}
#else
	{
		float32x2_t vlo = vget_low_f32(vmax);
		float32x2_t vhi = vget_high_f32(vmax);
		float32x2_t max0 = vpmax_f32(vlo, vhi);
		float32x2_t max1 = vpmax_f32(max0, max0); // Max is now at max1[0]
		vst1_lane_f32(maxf, max1, 0);
	}
#endif

	// Compute the min in register
#if (__aarch64__ == 1)
	{
		float min_val = vminvq_f32(vmin);
		*minf = min_val;
	}
#else
	{
		float32x2_t vlo = vget_low_f32(vmin);
		float32x2_t vhi = vget_high_f32(vmin);
		float32x2_t min0 = vpmin_f32(vlo, vhi);
		float32x2_t min1 = vpmin_f32(min0, min0); // min is now at min1[0]
		vst1_lane_f32(minf, min1, 0);
	}
#endif
}

/**
 * @brief Applies a scalar gain to a buffer of floats
 *
 * The mathematically equivalent operation is:
 *
 * dst[i] = src[i] * gain, for i = 0 to nframes
 *
 * The function uses NEON SIMD instructions to apply the gain to the buffer.
 * The function is optimized for both Aarch64 and Aarch32 platforms, and is
 * designed to deal with unaligned buffer.
 *
 * @param[in,out] dst Pointer to the buffer of floats
 * @param[in] nframes Number of frames in the buffer
 * @param[in] gain Gain to apply to the buffer
 */
C_FUNC void
arm_neon_apply_gain_to_buffer(float* dst, uint32_t nframes, float gain)
{
	float* dst_aligned = (float*) ALIGN_PTR_NEXT_16(dst);

	if (UNLIKELY(dst_aligned != dst))
	{
		size_t unaligned_count = dst_aligned - dst;
		size_t count = std::min<size_t>(unaligned_count, nframes);

		for (size_t i = 0; i < count; i++)
		{
			float32_t x0, y0;

			x0 = dst[i];
			y0 = x0 * gain;
			dst[i] = y0;
		}

		nframes -= count;
	}

	// Compute the number of SIMD frames
	size_t simd_count = nframes / 4;
	size_t nframes_simd = 4 * simd_count;
	size_t start = 0;

	float32x4_t g0 = vdupq_n_f32(gain);

	// Do some loop unrolling
	if (simd_count >= 4)
	{
		size_t unrolled_count = simd_count / 4;
		for (size_t i = start; i < unrolled_count; ++i)
		{
			size_t offset = 4 * 4 * i;
			float32x4_t x0, x1, x2, x3;
			float32x4_t y0, y1, y2, y3;

			float* ptr0 = dst_aligned + offset + (0 * 4);
			float* ptr1 = dst_aligned + offset + (1 * 4);
			float* ptr2 = dst_aligned + offset + (2 * 4);
			float* ptr3 = dst_aligned + offset + (3 * 4);

			x0 = vld1q_f32(ptr0);
			x1 = vld1q_f32(ptr1);
			x2 = vld1q_f32(ptr2);
			x3 = vld1q_f32(ptr3);

			y0 = vmulq_f32(x0, g0);
			y1 = vmulq_f32(x1, g0);
			y2 = vmulq_f32(x2, g0);
			y3 = vmulq_f32(x3, g0);

			vst1q_f32(ptr0, y0);
			vst1q_f32(ptr1, y1);
			vst1q_f32(ptr2, y2);
			vst1q_f32(ptr3, y3);
		}

		start = unrolled_count * 4;
	}

	// Do the remaining 4 samples at a time
	for (size_t i = start; i < simd_count; ++i)
	{
		size_t offset = 4 * i;
		float32x4_t x0, y0;

		x0 = vld1q_f32(dst_aligned + offset);
		y0 = vmulq_f32(x0, g0);
		vst1q_f32(dst_aligned + offset, y0);
	}

	// Do the remaining portion one sample at a time
	for (size_t frame = nframes_simd; frame < nframes; ++frame)
	{
		float32_t x0, y0;
		x0 = dst_aligned[frame];
		y0 = x0 * gain;
		dst_aligned[frame] = y0;
	}

	return;
}

/**
 * @brief Mixes the source buffer into the destination buffer with a gain
 * factor.
 *
 * This function is mathematically equivalent to:
 *
 * dst[i] = dst[i] + src[i] * gain, for i = 0 to nframes, a.k.a. @p saxpy
 *
 * It uses ARM NEON intrinsics for vectorized processing. Although on AArch64
 * pointer alignment is strictly not necessary, the performance is improved
 * when the buffers are aligned to 16-byte boundaries.
 *
 * @warning This function *WILL CRASH* on Aarch32 if the pointers are not
 * aligned to 16-byte boundaries.
 *
 * @param[in,out] dst Pointer to the destination buffer
 * @param[in] src Pointer to the source buffer
 * @param[in] nframes Number of frames (samples) to process
 * @param[in] gain Gain factor to apply to each element of the source buffer
 */
C_FUNC void
arm_neon_mix_buffers_with_gain(float* __restrict dst, const float* __restrict src, uint32_t nframes, float gain)
{
	size_t frame = 0;
	size_t num_frames = nframes;
	size_t n_frame16 = num_frames - (num_frames % 16);
	size_t n_frame4 = num_frames - (num_frames % 4);

	// Broadcast the same value over all lanes of SIMD
	float32x4_t vgain = vdupq_n_f32(gain);

	// Process blocks of 16 frames to utilize reasonable amount of
	// register file.

	while (frame < n_frame16)
	{
		const float* src_ptr = src + frame;
		float* dst_ptr = dst + frame;

		float32x4_t x0, x1, x2, x3;
		float32x4_t y0, y1, y2, y3;

		x0 = vld1q_f32(src_ptr + 0 * 4);
		x1 = vld1q_f32(src_ptr + 1 * 4);
		x2 = vld1q_f32(src_ptr + 2 * 4);
		x3 = vld1q_f32(src_ptr + 3 * 4);

		y0 = vld1q_f32(dst_ptr + 0 * 4);
		y1 = vld1q_f32(dst_ptr + 1 * 4);
		y2 = vld1q_f32(dst_ptr + 2 * 4);
		y3 = vld1q_f32(dst_ptr + 3 * 4);

		y0 = vmlaq_f32(y0, x0, vgain);
		y1 = vmlaq_f32(y1, x1, vgain);
		y2 = vmlaq_f32(y2, x2, vgain);
		y3 = vmlaq_f32(y3, x3, vgain);

		vst1q_f32(dst_ptr + 0 * 4, y0);
		vst1q_f32(dst_ptr + 1 * 4, y1);
		vst1q_f32(dst_ptr + 2 * 4, y2);
		vst1q_f32(dst_ptr + 3 * 4, y3);

		frame += 16;
	}

	// Process the remaining blocks 4 at a time if possible
	while (frame < n_frame4)
	{
		float32x4_t x0, y0;

		x0 = vld1q_f32(src + frame);
		y0 = vld1q_f32(dst + frame);
		y0 = vmlaq_f32(y0, x0, vgain);
		vst1q_f32(dst + frame, y0);

		frame += 4;
	}

	// Process the remaining samples 1 at a time
	while (frame < num_frames)
	{
		float32_t x0, y0;

		x0 = src[frame];
		y0 = dst[frame];
		// y0 = y0 + (x0 * gain);
		y0 = __builtin_fmaf(x0, gain, y0);
		dst[frame] = y0;

		++frame;
	}

	return;
}

/**
 * @brief Mixes the source buffer into the destination buffer without any gain
 * factor.
 *
 * Similar to @p arm_neon_mix_buffers_with_gain, this function is
 * mathematically equivalent to:
 *
 * dst[i] = dst[i] + src[i], for i = 0 to nframes
 *
 * Same as before, it works similarly as @p arm_neon_mix_buffers_with_gain, but
 * without any gain factor.
 *
 * @warning This function *WILL CRASH* on Aarch32 if the pointers are not
 * aligned to 16-byte boundaries.
 *
 * @param[in,out] dst Pointer to the destination buffer
 * @param[in] src Pointer to the source buffer
 * @param[in] nframes Number of frames (samples) to process
 * @param[in] gain Gain factor to apply to each element of the source buffer
 */
C_FUNC void
arm_neon_mix_buffers_no_gain(float* dst, const float* src, uint32_t nframes)
{
	// In Aarch64, alignment doesn't matter. But, this code will perform faster
	// with pointers aligned to 16 byte boundaries.

	size_t frame = 0;
	size_t num_frames = nframes;
	size_t n_frame16 = num_frames - (num_frames % 16);
	size_t n_frame4 = num_frames - (num_frames % 4);

	// Process blocks of 16 frames to utilize reasonable amount of
	// register file.

	while (frame < n_frame16)
	{
		const float* src_ptr = src + frame;
		float* dst_ptr = dst + frame;

		float32x4_t x0, x1, x2, x3;
		float32x4_t y0, y1, y2, y3;

		x0 = vld1q_f32(src_ptr + 0 * 4);
		x1 = vld1q_f32(src_ptr + 1 * 4);
		x2 = vld1q_f32(src_ptr + 2 * 4);
		x3 = vld1q_f32(src_ptr + 3 * 4);

		y0 = vld1q_f32(dst_ptr + 0 * 4);
		y1 = vld1q_f32(dst_ptr + 1 * 4);
		y2 = vld1q_f32(dst_ptr + 2 * 4);
		y3 = vld1q_f32(dst_ptr + 3 * 4);

		y0 = vaddq_f32(y0, x0);
		y1 = vaddq_f32(y1, x1);
		y2 = vaddq_f32(y2, x2);
		y3 = vaddq_f32(y3, x3);

		vst1q_f32(dst_ptr + 0 * 4, y0);
		vst1q_f32(dst_ptr + 1 * 4, y1);
		vst1q_f32(dst_ptr + 2 * 4, y2);
		vst1q_f32(dst_ptr + 3 * 4, y3);

		frame += 16;
	}

	// Process the remaining blocks 4 at a time if possible
	while (frame < n_frame4)
	{
		float32x4_t x0, y0;

		x0 = vld1q_f32(src + frame);
		y0 = vld1q_f32(dst + frame);
		y0 = vaddq_f32(y0, x0);
		vst1q_f32(dst + frame, y0);

		frame += 4;
	}

	// Process the remaining samples 1 at a time
	while (frame < num_frames)
	{
		float32_t x0, y0;

		x0 = src[frame];
		y0 = dst[frame];
		y0 += x0;
		dst[frame] = y0;

		++frame;
	}

	return;
}

/**
 * @brief Copies a buffer of floats from source to destination
 *
 * Similar to:
 * memcpy(dst, src, nframes * sizeof(float));
 *
 * @param[in,out] dst Pointer to the destination buffer
 * @param[in] src Pointer to the source buffer
 * @param[in] nframes Number of frames (samples) to process
 */
C_FUNC void
arm_neon_copy_vector(float* __restrict dst, const float* __restrict src, uint32_t nframes)
{
	// Use NEON when buffers are aligned
	do
	{
		while (nframes >= 16)
		{
			float32x4_t x0, x1, x2, x3;

			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);
			x2 = vld1q_f32(src + 8);
			x3 = vld1q_f32(src + 12);

			vst1q_f32(dst + 0, x0);
			vst1q_f32(dst + 4, x1);
			vst1q_f32(dst + 8, x2);
			vst1q_f32(dst + 12, x3);

			src += 16;
			dst += 16;
			nframes -= 16;
		}

		while (nframes >= 8)
		{
			float32x4_t x0, x1;

			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);

			vst1q_f32(dst + 0, x0);
			vst1q_f32(dst + 4, x1);

			src += 8;
			dst += 8;
			nframes -= 8;
		}

		while (nframes >= 4)
		{
			float32x4_t x0;

			x0 = vld1q_f32(src);
			vst1q_f32(dst, x0);

			src += 4;
			dst += 4;
			nframes -= 4;
		}

	} while (0);

	// Do the remaining samples
	while (nframes > 0)
	{
		*dst++ = *src++;
		--nframes;
	}
}

#endif
