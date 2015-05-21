/*
    Copyright (C) 2015 Paul Davis

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

#include "ardour/mix.h"

float
x86_sse_avx_compute_peak (const float * buf, uint32_t nsamples, float current)
{
	return default_compute_peak (buf, nsamples, current);
}

void
x86_sse_avx_apply_gain_to_buffer (float * buf, uint32_t nframes, float gain)
{
	default_apply_gain_to_buffer (buf, nframes, gain);
}

void
x86_sse_avx_mix_buffers_with_gain (float * dst, const float * src, uint32_t nframes, float gain)
{
	default_mix_buffers_with_gain (dst, src, nframes, gain);
}

void
x86_sse_avx_mix_buffers_no_gain (float * dst, const float * src, uint32_t nframes)
{
	default_mix_buffers_no_gain (dst, src, nframes);
}

void
x86_sse_avx_copy_vector (float * dst, const float * src, uint32_t nframes)
{
	default_copy_vector (dst, src, nframes);
}

void
x86_sse_avx_find_peaks (const float * buf, uint32_t nsamples, float *min, float *max)
{
	default_find_peaks (buf, nsamples, min, max);
}
