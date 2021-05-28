/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2015 Paul Davis <paul@linuxaudiosystems.com>
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
#ifndef __ardour_mix_h__
#define __ardour_mix_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/utils.h"

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)

extern "C" {
/* SSE functions */
	LIBARDOUR_API float x86_sse_compute_peak         (float const* buf, uint32_t nsamples, float current);
	LIBARDOUR_API void  x86_sse_apply_gain_to_buffer (float* buf, uint32_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_mix_buffers_with_gain(float* dst, float const* src, uint32_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_mix_buffers_no_gain  (float* dst, float const* src, uint32_t nframes);
}

LIBARDOUR_API void x86_sse_find_peaks              (float const* buf, uint32_t nsamples, float* min, float* max);

extern "C" {
/* AVX functions */
	LIBARDOUR_API float x86_sse_avx_compute_peak          (float const* buf, uint32_t nsamples, float current);
	LIBARDOUR_API void  x86_sse_avx_apply_gain_to_buffer  (float* buf, uint32_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_avx_mix_buffers_with_gain (float* dst, float const* src, uint32_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_avx_mix_buffers_no_gain   (float* dst, float const* src, uint32_t nframes);
	LIBARDOUR_API void  x86_sse_avx_copy_vector           (float* dst, float const* src, uint32_t nframes);
#ifndef PLATFORM_WINDOWS
	LIBARDOUR_API void  x86_sse_avx_find_peaks            (float const* buf, uint32_t nsamples, float* min, float* max);
#endif
}
#ifdef PLATFORM_WINDOWS
LIBARDOUR_API void x86_sse_avx_find_peaks               (float const* buf, uint32_t nsamples, float* min, float* max);
#endif

/* FMA functions */
#ifdef FPU_AVX_FMA_SUPPORT
LIBARDOUR_API void  x86_fma_mix_buffers_with_gain       (float* dst, float const* src, uint32_t nframes, float gain);
#endif

/* debug wrappers for SSE functions */

LIBARDOUR_API float debug_compute_peak               (ARDOUR::Sample const* buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void  debug_apply_gain_to_buffer       (ARDOUR::Sample* buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  debug_mix_buffers_with_gain      (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  debug_mix_buffers_no_gain        (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes);
LIBARDOUR_API void  debug_copy_vector                (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes);

#endif

#if defined (__APPLE__)

LIBARDOUR_API float veclib_compute_peak              (ARDOUR::Sample const* buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void  veclib_apply_gain_to_buffer      (ARDOUR::Sample* buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  veclib_mix_buffers_with_gain     (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  veclib_mix_buffers_no_gain       (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes);
LIBARDOUR_API void  veclib_find_peaks                (ARDOUR::Sample const* buf, ARDOUR::pframes_t nsamples, float* min, float* max);

#endif

#if defined ARM_NEON_SUPPORT
/* Optimized NEON functions */
extern "C" {
	LIBARDOUR_API float arm_neon_compute_peak          (float const* buf, uint32_t nsamples, float current);
	LIBARDOUR_API void  arm_neon_apply_gain_to_buffer  (float* buf, uint32_t nframes, float gain);
	LIBARDOUR_API void  arm_neon_copy_vector           (float* dst, float const* src, uint32_t nframes);
	LIBARDOUR_API void  arm_neon_find_peaks            (float const* src, uint32_t nframes, float* minf, float* maxf);
	LIBARDOUR_API void  arm_neon_mix_buffers_no_gain   (float* dst, float const* src, uint32_t nframes);
	LIBARDOUR_API void  arm_neon_mix_buffers_with_gain (float* dst, float const* src, uint32_t nframes, float gain);
}
#endif

/* non-optimized functions */

LIBARDOUR_API float default_compute_peak              (ARDOUR::Sample const* buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void  default_find_peaks                (ARDOUR::Sample const* buf, ARDOUR::pframes_t nsamples, float* min, float* max);
LIBARDOUR_API void  default_apply_gain_to_buffer      (ARDOUR::Sample* buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  default_mix_buffers_with_gain     (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  default_mix_buffers_no_gain       (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes);
LIBARDOUR_API void  default_copy_vector               (ARDOUR::Sample* dst, ARDOUR::Sample const* src, ARDOUR::pframes_t nframes);

#endif /* __ardour_mix_h__ */
