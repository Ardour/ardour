/*
 *  Copyright (C) 2002 Steve Harris <steve@plugin.org.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "gdither_types_internal.h"
#include "gdither.h"
#include "noise.h"

/* this monstrosity is necessary to get access to lrintf() and random().
   whoever is writing the glibc headers <cmath> and <cstdlib> should be
   hauled off to a programmer re-education camp. for the rest of
   their natural lives. or longer. <paul@linuxaudiosystems.com>
*/

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1
#ifdef __cplusplus
#include <cmath>
#else
#include <math.h>
#endif

#undef  __USE_SVID
#define __USE_SVID 1
#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

#include <sys/types.h>

/* Lipshitz's minimally audible FIR, only really works for 46kHz-ish signals */
static const float shaped_bs[] = { 2.033f, -2.165f, 1.959f, -1.590f, 0.6149f };

/* Some useful constants */
#define MAX_U8        255
#define MIN_U8          0
#define SCALE_U8      128.0f

#define MAX_S16     32767
#define MIN_S16    -32768
#define SCALE_S16   32768.0f

#define MAX_S24   8388607
#define MIN_S24  -8388608
#define SCALE_S24 8388608.0f

GDither gdither_new(GDitherType type, uint32_t channels,

		    GDitherSize bit_depth, int dither_depth)
{
    GDither s;

    s = (GDither)calloc(1, sizeof(struct GDither_s));
    s->type = type;
    s->channels = channels;
    s->bit_depth = (int)bit_depth;

    if (dither_depth <= 0 || dither_depth > (int)bit_depth) {
	dither_depth = (int)bit_depth;
    }
    s->dither_depth = dither_depth;

    s->scale = (float)(1LL << (dither_depth - 1));
    if (bit_depth == GDitherFloat || bit_depth == GDitherDouble) {
	s->post_scale_fp = 1.0f / s->scale;
	s->post_scale = 0;
    } else {
	s->post_scale_fp = 0.0f;
	s->post_scale = 1 << ((int)bit_depth - dither_depth);
    }

    switch (bit_depth) {
    case GDither8bit:
	/* Unsigned 8 bit */
	s->bias = 1.0f;
	s->clamp_u = 255;
	s->clamp_l = 0;
	break;
    case GDither16bit:
	/* Signed 16 bit */
	s->bias = 0.0f;
	s->clamp_u = 32767;
	s->clamp_l = -32768;
	break;
    case GDither32bit:
	/* Signed 24 bit, in upper 24 bits of 32 bit word */
	s->bias = 0.0f;
	s->clamp_u = 8388607;
	s->clamp_l = -8388608;
	break;
    case GDitherFloat:
	/* normalised float */
	s->bias = 0.0f;
	s->clamp_u = lrintf(s->scale);
	s->clamp_l = lrintf(-s->scale);
	break;
    case GDitherDouble:
	/* normalised float */
	s->bias = 0.0f;
	s->clamp_u = lrintf(s->scale);
	s->clamp_l = lrintf(-s->scale);
	break;
    case GDitherPerformanceTest:
	/* special performance test case */
	s->scale = SCALE_S24;
	s->post_scale = 256;
	s->bias = 0.0f;
	s->clamp_u = 8388607;
	s->clamp_l = -8388608;
	break;
    default:
	/* Not a bit depth we can handle */
	free(s);

	return NULL;
	break;
    }

    switch (type) {
    case GDitherNone:
    case GDitherRect:
	/* No state */
	break;

    case GDitherTri:
	/* The last whitenoise sample */
	s->tri_state = (float *) calloc(channels, sizeof(float));
	break;

    case GDitherShaped:
	/* The error from the last few samples encoded */
	s->shaped_state = (GDitherShapedState*)
			   calloc(channels, sizeof(GDitherShapedState));
	break;
    }

    return s;
}

void gdither_free(GDither s)
{
    if (s) {
	free(s->tri_state);
	free(s->shaped_state);
	free(s);
    }
}

inline static void gdither_innner_loop(const GDitherType dt,
    const uint32_t stride, const float bias, const float scale,

    const uint32_t post_scale, const int bit_depth,
    const uint32_t channel, const uint32_t length, float *ts,

    GDitherShapedState *ss, float const *x, void *y, const int clamp_u,

    const int clamp_l)
{
    uint32_t pos, i;
    uint8_t *o8 = (uint8_t*) y;
    int16_t *o16 = (int16_t*) y;
    int32_t *o32 = (int32_t*) y;
    float tmp, r, ideal;
    int64_t clamped;

    i = channel;
    for (pos = 0; pos < length; pos++, i += stride) {
	tmp = x[i] * scale + bias;

	switch (dt) {
	case GDitherNone:
	    break;
	case GDitherRect:
	    tmp -= GDITHER_NOISE;
	    break;
	case GDitherTri:
	    r = GDITHER_NOISE - 0.5f;
	    tmp -= r - ts[channel];
	    ts[channel] = r;
	    break;
	case GDitherShaped:
	    /* Save raw value for error calculations */
	    ideal = tmp;

	    /* Run FIR and add white noise */
	    ss->buffer[ss->phase] = GDITHER_NOISE * 0.5f;
	    tmp += ss->buffer[ss->phase] * shaped_bs[0]
		   + ss->buffer[(ss->phase - 1) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[1]
		   + ss->buffer[(ss->phase - 2) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[2]
		   + ss->buffer[(ss->phase - 3) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[3]
		   + ss->buffer[(ss->phase - 4) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[4];

	    /* Roll buffer and store last error */
	    ss->phase = (ss->phase + 1) & GDITHER_SH_BUF_MASK;
	    ss->buffer[ss->phase] = (float)lrintf(tmp) - ideal;
	    break;
	}

	clamped = lrintf(tmp);
	if (clamped > clamp_u) {
		clamped = clamp_u;
	} else if (clamped < clamp_l) {
		clamped = clamp_l;
	}

	switch (bit_depth) {
	case GDither8bit:
	    o8[i] = (u_int8_t) (clamped * post_scale);
	    break;
	case GDither16bit:
	    o16[i] = (int16_t) (clamped * post_scale);
	    break;
	case GDither32bit:
	    o32[i] = (int32_t) (clamped * post_scale);
	    break;
	}
    }
}

/* floating pint version of the inner loop function */
inline static void gdither_innner_loop_fp(const GDitherType dt,
    const uint32_t stride, const float bias, const float scale,

    const float post_scale, const int bit_depth,
    const uint32_t channel, const uint32_t length, float *ts,

    GDitherShapedState *ss, float const *x, void *y, const int clamp_u,

    const int clamp_l)
{
    uint32_t pos, i;
    float *oflt = (float*) y;
    double *odbl = (double*) y;
    float tmp, r, ideal;
    double clamped;

    i = channel;
    for (pos = 0; pos < length; pos++, i += stride) {
	tmp = x[i] * scale + bias;

	switch (dt) {
	case GDitherNone:
	    break;
	case GDitherRect:
	    tmp -= GDITHER_NOISE;
	    break;
	case GDitherTri:
	    r = GDITHER_NOISE - 0.5f;
	    tmp -= r - ts[channel];
	    ts[channel] = r;
	    break;
	case GDitherShaped:
	    /* Save raw value for error calculations */
	    ideal = tmp;

	    /* Run FIR and add white noise */
	    ss->buffer[ss->phase] = GDITHER_NOISE * 0.5f;
	    tmp += ss->buffer[ss->phase] * shaped_bs[0]
		   + ss->buffer[(ss->phase - 1) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[1]
		   + ss->buffer[(ss->phase - 2) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[2]
		   + ss->buffer[(ss->phase - 3) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[3]
		   + ss->buffer[(ss->phase - 4) & GDITHER_SH_BUF_MASK]
		     * shaped_bs[4];

	    /* Roll buffer and store last error */
	    ss->phase = (ss->phase + 1) & GDITHER_SH_BUF_MASK;
	    ss->buffer[ss->phase] = (float)lrintf(tmp) - ideal;
	    break;
	}

	clamped = rintf(tmp);
	if (clamped > clamp_u) {
		clamped = clamp_u;
	} else if (clamped < clamp_l) {
		clamped = clamp_l;
	}

	switch (bit_depth) {
	case GDitherFloat:
	    oflt[i] = (float) (clamped * post_scale);
	    break;
	case GDitherDouble:
	    odbl[i] = (double) (clamped * post_scale);
	    break;
	}
    }
}

#define GDITHER_CONV_BLOCK 512

void gdither_run(GDither s, uint32_t channel, uint32_t length,
                 double const *x, void *y)
{
    float conv[GDITHER_CONV_BLOCK];
    uint32_t i, pos;
    char *ycast = (char *)y;

    int step;

    switch (s->bit_depth) {
    case GDither8bit:
	step = 1;
	break;
    case GDither16bit:
	step = 2;
	break;
    case GDither32bit:
    case GDitherFloat:
	step = 4;
	break;
    case GDitherDouble:
	step = 8;
	break;
    default:
	step = 0;
	break;
    }

    pos = 0;
    while (pos < length) {
	for (i=0; (i + pos) < length && i < GDITHER_CONV_BLOCK; i++) {
	    conv[i] = x[pos + i];
	}
	gdither_runf(s, channel, i, conv, ycast + s->channels * step);
	pos += i;
    }
}

void gdither_runf(GDither s, uint32_t channel, uint32_t length,
                 float const *x, void *y)
{
    uint32_t pos, i;
    float tmp;
    int64_t clamped;
    GDitherShapedState *ss = NULL;

    if (!s || channel >= s->channels) {
	return;
    }

    if (s->shaped_state) {
	ss = s->shaped_state + channel;
    }

    if (s->type == GDitherNone && s->bit_depth == 23) {
	int32_t *o32 = (int32_t*) y;

        for (pos = 0; pos < length; pos++) {
            i = channel + (pos * s->channels);
            tmp = x[i] * 8388608.0f;

            clamped = lrintf(tmp);
            if (clamped > 8388607) {
                    clamped = 8388607;
            } else if (clamped < -8388608) {
                    clamped = -8388608;
            }

            o32[i] = (int32_t) (clamped * 256);
        }

        return;
    }

    /* some common case handling code - looks a bit wierd, but it allows
     * the compiler to optimise out the branches in the inner loop */
    if (s->bit_depth == 8 && s->dither_depth == 8) {
	switch (s->type) {
	case GDitherNone:
	    gdither_innner_loop(GDitherNone, s->channels, 128.0f, SCALE_U8,
				1, 8, channel, length, NULL, NULL, x, y,
				MAX_U8, MIN_U8);
	    break;
	case GDitherRect:
	    gdither_innner_loop(GDitherRect, s->channels, 128.0f, SCALE_U8,
				1, 8, channel, length, NULL, NULL, x, y,
				MAX_U8, MIN_U8);
	    break;
	case GDitherTri:
	    gdither_innner_loop(GDitherTri, s->channels, 128.0f, SCALE_U8,
				1, 8, channel, length, s->tri_state,
				NULL, x, y, MAX_U8, MIN_U8);
	    break;
	case GDitherShaped:
	    gdither_innner_loop(GDitherShaped, s->channels, 128.0f, SCALE_U8,
			        1, 8, channel, length, NULL,
				ss, x, y, MAX_U8, MIN_U8);
	    break;
	}
    } else if (s->bit_depth == 16 && s->dither_depth == 16) {
	switch (s->type) {
	case GDitherNone:
	    gdither_innner_loop(GDitherNone, s->channels, 0.0f, SCALE_S16,
				1, 16, channel, length, NULL, NULL, x, y,
				MAX_S16, MIN_S16);
	    break;
	case GDitherRect:
	    gdither_innner_loop(GDitherRect, s->channels, 0.0f, SCALE_S16,
				1, 16, channel, length, NULL, NULL, x, y,
				MAX_S16, MIN_S16);
	    break;
	case GDitherTri:
	    gdither_innner_loop(GDitherTri, s->channels, 0.0f, SCALE_S16,
				1, 16, channel, length, s->tri_state,
				NULL, x, y, MAX_S16, MIN_S16);
	    break;
	case GDitherShaped:
	    gdither_innner_loop(GDitherShaped, s->channels, 0.0f,
				SCALE_S16, 1, 16, channel, length, NULL,
				ss, x, y, MAX_S16, MIN_S16);
	    break;
	}
    } else if (s->bit_depth == 32 && s->dither_depth == 24) {
	switch (s->type) {
	case GDitherNone:
	    gdither_innner_loop(GDitherNone, s->channels, 0.0f, SCALE_S24,
				256, 32, channel, length, NULL, NULL, x,
				y, MAX_S24, MIN_S24);
	    break;
	case GDitherRect:
	    gdither_innner_loop(GDitherRect, s->channels, 0.0f, SCALE_S24,
				256, 32, channel, length, NULL, NULL, x,
				y, MAX_S24, MIN_S24);
	    break;
	case GDitherTri:
	    gdither_innner_loop(GDitherTri, s->channels, 0.0f, SCALE_S24,
				256, 32, channel, length, s->tri_state,
				NULL, x, y, MAX_S24, MIN_S24);
	    break;
	case GDitherShaped:
	    gdither_innner_loop(GDitherShaped, s->channels, 0.0f, SCALE_S24,
				256, 32, channel, length,
				NULL, ss, x, y, MAX_S24, MIN_S24);
	    break;
	}
    } else if (s->bit_depth == GDitherFloat || s->bit_depth == GDitherDouble) {
	gdither_innner_loop_fp(s->type, s->channels, s->bias, s->scale,
			    s->post_scale_fp, s->bit_depth, channel, length,
			    s->tri_state, ss, x, y, s->clamp_u, s->clamp_l);
    } else {
	/* no special case handling, just process it from the struct */

	gdither_innner_loop(s->type, s->channels, s->bias, s->scale,
			    s->post_scale, s->bit_depth, channel,
			    length, s->tri_state, ss, x, y, s->clamp_u,
			    s->clamp_l);
    }
}

/* vi:set ts=8 sts=4 sw=4: */
