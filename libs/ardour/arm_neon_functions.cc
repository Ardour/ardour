/*
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

#ifdef ARM_NEON_SUPPORT

#include <arm_acle.h>
#include <arm_neon.h>

#define IS_ALIGNED_TO(ptr, bytes) (((uintptr_t)ptr) % (bytes) == 0)

#ifdef __cplusplus
#define C_FUNC extern "C"
#else
#define C_FUNC
#endif

C_FUNC float
arm_neon_compute_peak(const float *src, uint32_t nframes, float current)
{
	float32x4_t vc0;

	// Broadcast single value to all elements of the register
	vc0 = vdupq_n_f32(current);

	// While pointer is not aligned, process one sample at a time
	while (!IS_ALIGNED_TO(src, sizeof(float32x4_t)) && (nframes > 0)) {
		float32x4_t x0;

		x0 = vld1q_dup_f32(src);
		x0 = vabsq_f32(x0);
		vc0 = vmaxq_f32(vc0, x0);

		++src;
		--nframes;
	}

	// SIMD portion with aligned buffers
	do {
		while (nframes >= 8) {
			float32x4_t x0, x1;

			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);

			x0 = vabsq_f32(x0);
			x1 = vabsq_f32(x1);

			vc0 = vmaxq_f32(vc0, x0);
			vc0 = vmaxq_f32(vc0, x1);

			src += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0;

			x0 = vld1q_f32(src);

			x0 = vabsq_f32(x0);
			vc0 = vmaxq_f32(vc0, x0);

			src += 4;
			nframes -= 4;
		}

		while (nframes >= 2) {
			float32x2_t x0;
			float32x4_t y0;


			x0 = vld1_f32(src);        // Load two elements
			x0 = vabs_f32(x0);         // Compute ABS value
			y0 = vcombine_f32(x0, x0); // Combine two vectors

			vc0 = vmaxq_f32(vc0, y0);

			src += 2;
			nframes -= 2;
		}
	} while (0);


	// Do remaining samples one frame at a time
	while (nframes > 0) {
		float32x4_t x0;

		x0 = vld1q_dup_f32(src);
		x0 = vabsq_f32(x0);
		vc0 = vmaxq_f32(vc0, x0);

		++src;
		--nframes;
	}

	// Compute the max in register
	do {
		float32x2_t vlo = vget_low_f32(vc0);
		float32x2_t vhi = vget_high_f32(vc0);
		float32x2_t max0 = vpmax_f32(vlo, vhi);
		float32x2_t max1 = vpmax_f32(max0, max0); // Max is now at max1[0]
		current = vget_lane_f32(max1, 0);
	} while (0);

	return current;
}

C_FUNC void
arm_neon_find_peaks(const float *src, uint32_t nframes, float *minf, float *maxf)
{
	float32x4_t vmin, vmax;

	// Broadcast single value to all elements of the register
	vmin = vld1q_dup_f32(minf);
	vmax = vld1q_dup_f32(maxf);

	// While pointer is not aligned, process one sample at a time
	while (!IS_ALIGNED_TO(src, sizeof(float32x4_t)) && (nframes > 0)) {
		float32x4_t x0;

		x0 = vld1q_dup_f32(src);
		vmax = vmaxq_f32(vmax, x0);
		vmin = vminq_f32(vmin, x0);

		++src;
		--nframes;
	}

	// SIMD portion with aligned buffers
	do {
		while (nframes >= 8) {
			float32x4_t x0, x1;

			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);

			vmax = vmaxq_f32(vmax, x0);
			vmax = vmaxq_f32(vmax, x1);

			vmin = vminq_f32(vmin, x0);
			vmin = vminq_f32(vmin, x1);

			src += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0;

			x0 = vld1q_f32(src);

			vmax = vmaxq_f32(vmax, x0);
			vmin = vminq_f32(vmin, x0);

			src += 4;
			nframes -= 4;
		}

		while (nframes >= 2) {
			float32x2_t x0;
			float32x4_t y0;


			x0 = vld1_f32(src);        // Load two elements
			y0 = vcombine_f32(x0, x0); // Combine two vectors

			vmax = vmaxq_f32(vmax, y0);
			vmin = vminq_f32(vmin, y0);

			src += 2;
			nframes -= 2;
		}
	} while (0);


	// Do remaining samples one frame at a time
	while (nframes > 0) {
		float32x4_t x0;

		x0 = vld1q_dup_f32(src);
		vmax = vmaxq_f32(vmax, x0);
		vmin = vminq_f32(vmin, x0);

		++src;
		--nframes;
	}

	// Compute the max in register
	do {
		float32x2_t vlo = vget_low_f32(vmax);
		float32x2_t vhi = vget_high_f32(vmax);
		float32x2_t max0 = vpmax_f32(vlo, vhi);
		float32x2_t max1 = vpmax_f32(max0, max0); // Max is now at max1[0]
		vst1_lane_f32(maxf, max1, 0);
	} while (0);

	// Compute the min in register
	do {
		float32x2_t vlo = vget_low_f32(vmin);
		float32x2_t vhi = vget_high_f32(vmin);
		float32x2_t min0 = vpmin_f32(vlo, vhi);
		float32x2_t min1 = vpmin_f32(min0, min0); // min is now at min1[0]
		vst1_lane_f32(minf, min1, 0);
	} while (0);
}

C_FUNC void
arm_neon_apply_gain_to_buffer(float *dst, uint32_t nframes, float gain)
{
	while (!IS_ALIGNED_TO(dst, sizeof(float32x4_t)) && nframes > 0) {
		float32_t x0, y0;

		x0 = *dst;
		y0 = x0 * gain;
		*dst = y0;

		++dst;
		--nframes;
	}

	// SIMD portion with aligned buffers
	do {
		float32x4_t g0 = vdupq_n_f32(gain);

		while (nframes >= 8) {
			float32x4_t x0, x1, y0, y1;
			x0 = vld1q_f32(dst + 0);
			x1 = vld1q_f32(dst + 4);

			y0 = vmulq_f32(x0, g0);
			y1 = vmulq_f32(x1, g0);

			vst1q_f32(dst + 0, y0);
			vst1q_f32(dst + 4, y1);

			dst += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0, y0;

			x0 = vld1q_f32(dst);
			y0 = vmulq_f32(x0, g0);
			vst1q_f32(dst, y0);

			dst += 4;
			nframes -= 4;
		}

		while (nframes >= 2) {
			float32x2_t x0, y0;

			x0 = vld1_f32(dst);
			y0 = vmul_n_f32(x0, gain);
			vst1_f32(dst, y0);

			dst += 2;
			nframes -= 2;
		}
	} while (0);

	// Do the remaining portion one sample at a time
	while (nframes > 0) {
		float32_t x0, y0;

		x0 = *dst;
		y0 = x0 * gain;
		*dst = y0;

		++dst;
		--nframes;
	}
}

C_FUNC void
arm_neon_mix_buffers_with_gain(
	float *__restrict dst, const float *__restrict src,
	uint32_t nframes, float gain)
{
	// While buffers aren't aligned, then process one sample at a time
	while (!(IS_ALIGNED_TO(src, sizeof(float32x4_t)) &&
			IS_ALIGNED_TO(dst, sizeof(float32x4_t))) &&
			(nframes > 0)) {
		float32_t x0, y0;

		x0 = *src;
		y0 = *dst;
		y0 = y0 + (x0 * gain);
		*dst = y0;

		++dst;
		++src;
		--nframes;
	}

	// Use NEON when buffers are aligned
	do {
		float32x4_t g0 = vdupq_n_f32(gain);

		while (nframes >= 8) {
			float32x4_t x0, x1, y0, y1;
			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);
			y0 = vld1q_f32(dst + 0);
			y1 = vld1q_f32(dst + 4);

			y0 = vmlaq_f32(y0, x0, g0);
			y1 = vmlaq_f32(y1, x1, g0);

			vst1q_f32(dst + 0, y0);
			vst1q_f32(dst + 4, y1);

			src += 8;
			dst += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0, y0;
			x0 = vld1q_f32(src);
			y0 = vld1q_f32(dst);

			y0 = vmlaq_f32(y0, x0, g0);

			vst1q_f32(dst, y0);

			src += 4;
			dst += 4;
			nframes -= 4;
		}

		while (nframes >= 2) {
			float32x2_t x0, y0;
			x0 = vld1_f32(src);
			y0 = vld1_f32(dst);

			y0 = vmla_n_f32(y0, x0, gain);

			vst1_f32(dst, y0);

			src += 2;
			dst += 2;
			nframes -= 2;
		}
	} while (0);

	// Do the remaining samples
	while (nframes > 0) {

		float32_t x0, y0;

		x0 = *src;
		y0 = *dst;
		y0 = y0 + (x0 * gain);
		*dst = y0;

		++dst;
		++src;
		--nframes;
	}
}

C_FUNC void
arm_neon_mix_buffers_no_gain(float *dst, const float *src, uint32_t nframes)
{
	// While buffers aren't aligned, then process one sample at a time
	while (!(IS_ALIGNED_TO(src, sizeof(float32x4_t)) &&
			IS_ALIGNED_TO(dst, sizeof(float32x4_t))) &&
			(nframes > 0)) {
		float32_t x0, y0;

		x0 = *src;
		y0 = *dst;
		y0 = y0 + x0;
		*dst = y0;

		++src;
		++dst;
		--nframes;
	}

	// Use NEON when buffers are aligned
	do {
		while (nframes >= 8) {
			float32x4_t x0, x1, y0, y1;
			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);
			y0 = vld1q_f32(dst + 0);
			y1 = vld1q_f32(dst + 4);

			y0 = vaddq_f32(y0, x0);
			y1 = vaddq_f32(y1, x1);

			vst1q_f32(dst + 0, y0);
			vst1q_f32(dst + 4, y1);

			src += 8;
			dst += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0, y0;

			x0 = vld1q_f32(src);
			y0 = vld1q_f32(dst);

			y0 = vaddq_f32(y0, x0);

			vst1q_f32(dst, y0);

			src += 4;
			dst += 4;
			nframes -= 4;
		}
	} while (0);

	// Do the remaining samples
	while (nframes > 0) {
		float32_t x0, y0;

		x0 = *src;
		y0 = *dst;
		y0 = y0 + x0;
		*dst = y0;

		++src;
		++dst;
		--nframes;
	}
}

C_FUNC void
arm_neon_copy_vector(
	float *__restrict dst, const float *__restrict src,
	uint32_t nframes)
{
	// While buffers aren't aligned, then process one sample at a time
	while (!(IS_ALIGNED_TO(src, sizeof(float32x4_t)) &&
			IS_ALIGNED_TO(dst, sizeof(float32x4_t))) &&
			(nframes > 0)) {
		*dst++ = *src++;
		--nframes;
	}

	// Use NEON when buffers are aligned
	do {
		while (nframes >= 16) {
			float32x4_t x0, x1, x2, x3;

			x0 = vld1q_f32(src + 0 );
			x1 = vld1q_f32(src + 4 );
			x2 = vld1q_f32(src + 8 );
			x3 = vld1q_f32(src + 12);

			vst1q_f32(dst + 0 , x0);
			vst1q_f32(dst + 4 , x1);
			vst1q_f32(dst + 8 , x2);
			vst1q_f32(dst + 12, x3);

			src += 16;
			dst += 16;
			nframes -= 16;
		}

		while (nframes >= 8) {
			float32x4_t x0, x1;

			x0 = vld1q_f32(src + 0);
			x1 = vld1q_f32(src + 4);

			vst1q_f32(dst + 0, x0);
			vst1q_f32(dst + 4, x1);

			src += 8;
			dst += 8;
			nframes -= 8;
		}

		while (nframes >= 4) {
			float32x4_t x0;

			x0 = vld1q_f32(src);
			vst1q_f32(dst, x0);

			src += 4;
			dst += 4;
			nframes -= 4;
		}

	} while (0);

	// Do the remaining samples
	while (nframes > 0) {
		*dst++ = *src++;
		--nframes;
	}
}

#endif
