/*
    Copyright (C) 2000-2005 Paul Davis

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

#include <cmath>
#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/mix.h"
#include "ardour/runtime_functions.h"
#include <stdint.h>

using std::min;
using std::max;
using namespace ARDOUR;

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)
// Debug wrappers

float
debug_compute_peak (ARDOUR::Sample *buf, pframes_t nsamples, float current)
{
	if ( ((intptr_t)buf % 16) != 0) {
		std::cerr << "compute_peak(): buffer unaligned!" << std::endl;
	}

	return x86_sse_compute_peak(buf, nsamples, current);
}

void
debug_apply_gain_to_buffer (ARDOUR::Sample *buf, pframes_t nframes, float gain)
{
	if ( ((intptr_t)buf % 16) != 0) {
		std::cerr << "apply_gain_to_buffer(): buffer unaligned!" << std::endl;
	}

	x86_sse_apply_gain_to_buffer(buf, nframes, gain);
}

void
debug_mix_buffers_with_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, pframes_t nframes, float gain)
{
	if ( ((intptr_t)dst & 15) != 0) {
		std::cerr << "mix_buffers_with_gain(): dst unaligned!" << std::endl;
	}

	if ( ((intptr_t)dst & 15) != ((intptr_t)src & 15) ) {
		std::cerr << "mix_buffers_with_gain(): dst & src don't have the same alignment!" << std::endl;
		mix_buffers_with_gain(dst, src, nframes, gain);
	} else {
		x86_sse_mix_buffers_with_gain(dst, src, nframes, gain);
	}
}

void
debug_mix_buffers_no_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, pframes_t nframes)
{
	if ( ((intptr_t)dst & 15) != 0) {
		std::cerr << "mix_buffers_no_gain(): dst unaligned!" << std::endl;
	}

	if ( ((intptr_t)dst & 15) != ((intptr_t)src & 15) ) {
		std::cerr << "mix_buffers_no_gain(): dst & src don't have the same alignment!" << std::endl;
		mix_buffers_no_gain(dst, src, nframes);
	} else {
		x86_sse_mix_buffers_no_gain(dst, src, nframes);
	}
}

#endif


float
default_compute_peak (const ARDOUR::Sample * buf, pframes_t nsamples, float current)
{
	for (pframes_t i = 0; i < nsamples; ++i) {
		current = f_max (current, fabsf (buf[i]));
	}

	return current;
}

void
default_find_peaks (const ARDOUR::Sample * buf, pframes_t nframes, float *minf, float *maxf)
{
	pframes_t i;
	float a, b;

	a = *maxf;
	b = *minf;

	for (i = 0; i < nframes; i++)
	{
		a = max (buf[i], a);
		b = min (buf[i], b);
	}

	*maxf = a;
	*minf = b;
}

void
default_apply_gain_to_buffer (ARDOUR::Sample * buf, pframes_t nframes, float gain)
{
	for (pframes_t i=0; i<nframes; i++)
		buf[i] *= gain;
}

void
default_mix_buffers_with_gain (ARDOUR::Sample * dst, const ARDOUR::Sample * src, pframes_t nframes, float gain)
{
	for (pframes_t i = 0; i < nframes; i++) {
		dst[i] += src[i] * gain;
	}
}

void
default_mix_buffers_no_gain (ARDOUR::Sample * dst, const ARDOUR::Sample * src, pframes_t nframes)
{
	for (pframes_t i=0; i < nframes; i++) {
		dst[i] += src[i];
	}
}

#if defined (__APPLE__) && defined (BUILD_VECLIB_OPTIMIZATIONS)
#include <Accelerate/Accelerate.h>

float
veclib_compute_peak (const ARDOUR::Sample * buf, pframes_t nsamples, float current)
{
	float tmpmax = 0.0f;
	vDSP_maxmgv(buf, 1, &tmpmax, nsamples);
	return f_max(current, tmpmax);
}

void
veclib_find_peaks (const ARDOUR::Sample * buf, pframes_t nframes, float *min, float *max)
{
	vDSP_maxv (const_cast<ARDOUR::Sample*>(buf), 1, max, nframes);
	vDSP_minv (const_cast<ARDOUR::Sample*>(buf), 1, min, nframes);
}

void
veclib_apply_gain_to_buffer (ARDOUR::Sample * buf, pframes_t nframes, float gain)
{
	vDSP_vsmul(buf, 1, &gain, buf, 1, nframes);
}

void
veclib_mix_buffers_with_gain (ARDOUR::Sample * dst, const ARDOUR::Sample * src, pframes_t nframes, float gain)
{
	vDSP_vsma(src, 1, &gain, dst, 1, dst, 1, nframes);
}

void
veclib_mix_buffers_no_gain (ARDOUR::Sample * dst, const ARDOUR::Sample * src, pframes_t nframes)
{
	// It seems that a vector mult only operation does not exist...
	float gain = 1.0f;
	vDSP_vsma(src, 1, &gain, dst, 1, dst, 1, nframes);
}

#endif


