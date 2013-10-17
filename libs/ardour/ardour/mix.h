/*
    Copyright (C) 2005 Sampo Savolainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#ifndef __ardour_mix_h__
#define __ardour_mix_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/utils.h"

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)

extern "C" {
/* SSE functions */
	LIBARDOUR_API float x86_sse_compute_peak         (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float current);
	LIBARDOUR_API void  x86_sse_apply_gain_to_buffer (ARDOUR::Sample * buf, ARDOUR::pframes_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_mix_buffers_with_gain(ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes, float gain);
	LIBARDOUR_API void  x86_sse_mix_buffers_no_gain  (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes);
}

LIBARDOUR_API void  x86_sse_find_peaks               (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float *min, float *max);

/* debug wrappers for SSE functions */

LIBARDOUR_API float debug_compute_peak               (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void  debug_apply_gain_to_buffer       (ARDOUR::Sample * buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  debug_mix_buffers_with_gain      (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  debug_mix_buffers_no_gain        (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes);

#endif

#if defined (__APPLE__)

LIBARDOUR_API float veclib_compute_peak              (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void veclib_find_peaks                 (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float *min, float *max);
LIBARDOUR_API void  veclib_apply_gain_to_buffer      (ARDOUR::Sample * buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  veclib_mix_buffers_with_gain     (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  veclib_mix_buffers_no_gain       (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes);

#endif

/* non-optimized functions */

LIBARDOUR_API float default_compute_peak              (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float current);
LIBARDOUR_API void  default_find_peaks                (const ARDOUR::Sample * buf, ARDOUR::pframes_t nsamples, float *min, float *max);
LIBARDOUR_API void  default_apply_gain_to_buffer      (ARDOUR::Sample * buf, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  default_mix_buffers_with_gain     (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes, float gain);
LIBARDOUR_API void  default_mix_buffers_no_gain       (ARDOUR::Sample * dst, const ARDOUR::Sample * src, ARDOUR::pframes_t nframes);

#endif /* __ardour_mix_h__ */
