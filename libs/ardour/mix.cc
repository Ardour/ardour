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

    $Id$
*/

#include <cmath>
#include <ardour/types.h>
#include <ardour/utils.h>
#include <ardour/mix.h>
#include <stdint.h>

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)

// Debug wrappers

float
debug_compute_peak (ARDOUR::Sample *buf, jack_nframes_t nsamples, float current) 
{
	if ( ((intptr_t)buf % 16) != 0) {
		cerr << "compute_peak(): buffer unaligned!" << endl;
	}

	return x86_sse_compute_peak(buf, nsamples, current);
}

void
debug_apply_gain_to_buffer (ARDOUR::Sample *buf, jack_nframes_t nframes, float gain)
{
	if ( ((intptr_t)buf % 16) != 0) {
		cerr << "apply_gain_to_buffer(): buffer unaligned!" << endl;
	}

	x86_sse_apply_gain_to_buffer(buf, nframes, gain);
}

void
debug_mix_buffers_with_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes, float gain)
{
	if ( ((intptr_t)dst & 15) != 0) {
		cerr << "mix_buffers_with_gain(): dst unaligned!" << endl;
	}

	if ( ((intptr_t)dst & 15) != ((intptr_t)src & 15) ) {
		cerr << "mix_buffers_with_gain(): dst & src don't have the same alignment!" << endl;
		mix_buffers_with_gain(dst, src, nframes, gain);
	} else {
		x86_sse_mix_buffers_with_gain(dst, src, nframes, gain);
	}
}

void
debug_mix_buffers_no_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes)
{
	if ( ((intptr_t)dst & 15) != 0) {
		cerr << "mix_buffers_no_gain(): dst unaligned!" << endl;
	}

	if ( ((intptr_t)dst & 15) != ((intptr_t)src & 15) ) {
		cerr << "mix_buffers_no_gain(): dst & src don't have the same alignment!" << endl;
		mix_buffers_no_gain(dst, src, nframes);
	} else {
		x86_sse_mix_buffers_no_gain(dst, src, nframes);
	}
}

#endif


float
compute_peak (ARDOUR::Sample *buf, jack_nframes_t nsamples, float current) 
{
	for (jack_nframes_t i = 0; i < nsamples; ++i) {
		current = f_max (current, fabsf (buf[i]));
	}

	return current;
}	

void
apply_gain_to_buffer (ARDOUR::Sample *buf, jack_nframes_t nframes, float gain)
{		
	for (jack_nframes_t i=0; i<nframes; i++)
		buf[i] *= gain;
}

void
mix_buffers_with_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes, float gain)
{
	for (jack_nframes_t i = 0; i < nframes; i++) {
		dst[i] += src[i] * gain;
	}
}

void
mix_buffers_no_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes)
{
	for (jack_nframes_t i=0; i < nframes; i++) {
		dst[i] += src[i];
	}
}

#if defined (__APPLE__) && defined (BUILD_VECLIB_OPTIMIZATIONS)
#include <Accelerate/Accelerate.h>

float
veclib_compute_peak (ARDOUR::Sample *buf, jack_nframes_t nsamples, float current)
{
	float tmpmax = 0.0f;
        vDSP_maxmgv(buf, 1, &tmpmax, nsamples);
        return f_max(current, tmpmax);
}

void
veclib_apply_gain_to_buffer (ARDOUR::Sample *buf, jack_nframes_t nframes, float gain)
{
        vDSP_vsmul(buf, 1, &gain, buf, 1, nframes);
}

void
veclib_mix_buffers_with_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes, float gain)
{
        vDSP_vsma(src, 1, &gain, dst, 1, dst, 1, nframes);
}

void
veclib_mix_buffers_no_gain (ARDOUR::Sample *dst, ARDOUR::Sample *src, jack_nframes_t nframes)
{
        // It seems that a vector mult only operation does not exist...
        float gain = 1.0f;
        vDSP_vsma(src, 1, &gain, dst, 1, dst, 1, nframes);
}

#endif
		

