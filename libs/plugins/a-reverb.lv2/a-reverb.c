/*
 * Copyright (C) 2003-2004 Fredrik Kilander <fk@dsv.su.se>
 * Copyright (C) 2008-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Will Panther <pantherb@setbfree.org>
 * Copyright (C) 2016 Damien Zammit <damien@zamaudio.com>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for M_PI
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#define RV_NZ 7
#define DENORMAL_PROTECT (1e-14)

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

typedef struct {
	float* delays[2][RV_NZ]; /**< delay line buffer */
	size_t size[2][RV_NZ];

	float* idx0[2][RV_NZ];	/**< Reset pointer ref delays[]*/
	float* idxp[2][RV_NZ];	/**< Index pointer ref delays[]*/
	float* endp[2][RV_NZ];	/**< End pointer   ref delays[]*/

	float gain[RV_NZ]; /**< feedback gains */
	float yy1_0; /**< Previous output sample */
	float y_1_0; /**< Feedback sample */
	float yy1_1; /**< Previous output sample */
	float y_1_1; /**< Feedback sample */

	int end[2][RV_NZ];

	float inputGain;	/**< Input gain value */
	float fbk;	/**< Feedback gain */
	float wet;	/**< Output dry gain */
	float dry;	/**< Output wet gain */
} b_reverb;

static int
setReverbPointers (b_reverb *r, int i, int c, const double rate)
{
	int e = (r->end[c][i] * rate / 25000.0);
	e = e | 1;
	r->size[c][i] = e + 2;
	r->delays[c][i] = (float*)realloc ((void*)r->delays[c][i], r->size[c][i] * sizeof (float));
	if (!r->delays[c][i]) {
		return -1;
	} else {
		memset (r->delays[c][i], 0 , r->size[c][i] * sizeof (float));
	}
	r->endp[c][i] = r->delays[c][i] + e + 1;
	r->idx0[c][i] = r->idxp[c][i] = &(r->delays[c][i][0]);

	return 0;
}

static int
initReverb (b_reverb *r, const double rate)
{
	int err = 0;
	int stereowidth = 7;

	r->inputGain = powf (10.0, .05 * -20.0);  // -20dB
	r->fbk = -0.015; /* Feedback gain */
	r->wet = 0.3;
	r->dry = 0.7;

	/* feedback combfilter */
	r->gain[0] = 0.773;
	r->gain[1] = 0.802;
	r->gain[2] = 0.753;
	r->gain[3] = 0.733;

	/* all-pass filter */
	r->gain[4] = sqrtf (0.5);
	r->gain[5] = sqrtf (0.5);
	r->gain[6] = sqrtf (0.5);

	/* delay lines left */
	r->end[0][0] = 1687;
	r->end[0][1] = 1601;
	r->end[0][2] = 2053;
	r->end[0][3] = 2251;

	/* all pass filters left */
	r->end[0][4] = 347;
	r->end[0][5] = 113;
	r->end[0][6] = 37;

	/* delay lines right */
	r->end[1][0] = 1687 + stereowidth;
	r->end[1][1] = 1601 + stereowidth;
	r->end[1][2] = 2053 + stereowidth;
	r->end[1][3] = 2251 + stereowidth;

	/* all pass filters right */
	r->end[1][4] = 347 + stereowidth;
	r->end[1][5] = 113 + stereowidth;
	r->end[1][6] = 37 + stereowidth;

	for (int i = 0; i < RV_NZ; ++i) {
		r->delays[0][i] = NULL;
		r->delays[1][i] = NULL;
	}

	r->yy1_0 = 0.0;
	r->y_1_0 = 0.0;
	r->yy1_1 = 0.0;
	r->y_1_1 = 0.0;

	for (int i = 0; i < RV_NZ; ++i) {
		err |= setReverbPointers (r, i, 0, rate);
		err |= setReverbPointers (r, i, 1, rate);
	}
	return err;
}

static void
reverb (b_reverb* r,
        const float* inbuf0,
        const float* inbuf1,
        float* outbuf0,
        float* outbuf1,
        size_t n_samples)
{
	float** const idxp0 = r->idxp[0];
	float** const idxp1 = r->idxp[1];
	float* const* const endp0 = r->endp[0];
	float* const* const endp1 = r->endp[1];
	float* const* const idx00 = r->idx0[0];
	float* const* const idx01 = r->idx0[1];
	const float* const gain = r->gain;
	const float inputGain = r->inputGain;
	const float fbk = r->fbk;
	const float wet = r->wet;
	const float dry = r->dry;

	const float* xp0 = inbuf0;
	const float* xp1 = inbuf1;
	float* yp0 = outbuf0;
	float* yp1 = outbuf1;

	float y_1_0 = r->y_1_0;
	float yy1_0 = r->yy1_0;
	float y_1_1 = r->y_1_1;
	float yy1_1 = r->yy1_1;

	for (size_t i = 0; i < n_samples; ++i) {
		int j;
		float y;
		float xo0 = *xp0++;
		float xo1 = *xp1++;
		if (!isfinite_local(xo0) || fabsf (xo0) > 10.f) { xo0 = 0; }
		if (!isfinite_local(xo1) || fabsf (xo1) > 10.f) { xo1 = 0; }
		xo0 += DENORMAL_PROTECT;
		xo1 += DENORMAL_PROTECT;
		const float x0 = y_1_0 + (inputGain * xo0);
		const float x1 = y_1_1 + (inputGain * xo1);

		float xa = 0.0;
		float xb = 0.0;
		/* First we do four feedback comb filters (ie parallel delay lines,
		 * each with a single tap at the end that feeds back at the start) */

		for (j = 0; j < 4; ++j) {
			y = *idxp0[j];
			*idxp0[j] = x0 + (gain[j] * y);
			if (endp0[j] <= ++(idxp0[j])) {
				idxp0[j] = idx00[j];
			}
			xa += y;
		}
		for (; j < 7; ++j) {
			y = *idxp0[j];
			*idxp0[j] = gain[j] * (xa + y);
			if (endp0[j] <= ++(idxp0[j])) {
				idxp0[j] = idx00[j];
			}
			xa = y - xa;
		}

		y = 0.5f * (xa + yy1_0);
		yy1_0 = y;
		y_1_0 = fbk * xa;

		*yp0++ = ((wet * y) + (dry * xo0));

		for (j = 0; j < 4; ++j) {
			y = *idxp1[j];
			*idxp1[j] = x1 + (gain[j] * y);
			if (endp1[j] <= ++(idxp1[j])) {
				idxp1[j] = idx01[j];
			}
			xb += y;
		}
		for (; j < 7; ++j) {
			y = *idxp1[j];
			*idxp1[j] = gain[j] * (xb + y);
			if (endp1[j] <= ++(idxp1[j])) {
				idxp1[j] = idx01[j];
			}
			xb = y - xb;
		}

		y = 0.5f * (xb + yy1_1);
		yy1_1 = y;
		y_1_1 = fbk * xb;

		*yp1++ = ((wet * y) + (dry * xo1));
	}

	if (!isfinite_local(y_1_0)) { y_1_0 = 0; }
	if (!isfinite_local(yy1_1)) { yy1_0 = 0; }
	if (!isfinite_local(y_1_1)) { y_1_1 = 0; }
	if (!isfinite_local(yy1_1)) { yy1_1 = 0; }

	r->y_1_0 = y_1_0 + DENORMAL_PROTECT;
	r->yy1_0 = yy1_0 + DENORMAL_PROTECT;
	r->y_1_1 = y_1_1 + DENORMAL_PROTECT;
	r->yy1_1 = yy1_1 + DENORMAL_PROTECT;
}

/******************************************************************************
 * LV2 wrapper
 */

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

typedef enum {
	AR_INPUT0     = 0,
	AR_INPUT1     = 1,
	AR_OUTPUT0    = 2,
	AR_OUTPUT1    = 3,
	AR_MIX        = 4,
	AR_ROOMSZ     = 5,
	AR_ENABLE     = 6,
} PortIndex;

typedef struct {
	float* input0;
	float* input1;
	float* output0;
	float* output1;

	float* mix;
	float* roomsz;
	float* enable;

	float v_mix;
	float v_roomsz;
	float srate;
	float tau;

	b_reverb r;
} AReverb;

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	AReverb* self = (AReverb*)calloc (1, sizeof (AReverb));
	if (!self) {
		return NULL;
	}
	if (initReverb (&self->r, rate)) {
		return NULL;
	}

	// these are set in initReverb()
	self->v_roomsz = 0.75;
	self->v_mix = 0.1;
	self->srate = rate;
	self->tau = 1.f - expf (-2.f * M_PI * 64.f * 15.f / self->srate); // 15Hz, 64fpp

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	AReverb* self = (AReverb*)instance;

	switch ((PortIndex)port) {
		case AR_INPUT0:
			self->input0 = (float*)data;
			break;
		case AR_INPUT1:
			self->input1 = (float*)data;
			break;
		case AR_OUTPUT0:
			self->output0 = (float*)data;
			break;
		case AR_OUTPUT1:
			self->output1 = (float*)data;
			break;
		case AR_MIX:
			self->mix = (float*)data;
			break;
		case AR_ROOMSZ:
			self->roomsz = (float*)data;
			break;
		case AR_ENABLE:
			self->enable = (float*)data;
			break;
	}
}

static void
activate (LV2_Handle instance)
{
	AReverb* self = (AReverb*)instance;

	self->r.y_1_0 = 0;
	self->r.yy1_0 = 0;
	self->r.y_1_1 = 0;
	self->r.yy1_1 = 0;
	for (int i = 0; i < RV_NZ; ++i) {
		for (int c = 0; c < 2; ++c) {
			memset (self->r.delays[c][i], 0, self->r.size[c][i] * sizeof (float));
		}
	}
}

static void
deactivate (LV2_Handle instance)
{
	activate(instance);
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	AReverb* self = (AReverb*)instance;

	const float* const input0 = self->input0;
	const float* const input1 = self->input1;
	float* const      output0 = self->output0;
	float* const      output1 = self->output1;

	const float tau = self->tau;
	const float mix = *self->enable <= 0 ? 0 : *self->mix;

	uint32_t remain = n_samples;
	uint32_t offset = 0;
	uint32_t iterpolate = 0;

	if (fabsf (mix - self->v_mix) < .01) { // 40dB
		if (self->v_mix != mix && *self->enable <= 0) {
			// entering bypass, reset reverb
			activate (self);
		}
		self->v_mix = mix;
		self->r.wet = self->v_mix;
		self->r.dry = 1.0 - self->v_mix;
	} else {
		iterpolate |= 1;
	}

	if (fabsf (*self->roomsz  - self->v_roomsz) < .01) {
		self->v_roomsz = *self->roomsz;
	} else {
		iterpolate |= 2;
	}

	while (remain > 0) {
		uint32_t p_samples = remain;
		if (iterpolate && p_samples > 64) {
			p_samples = 64;
		}

		if (iterpolate & 1) {
			self->v_mix += tau * (mix - self->v_mix);
			self->r.wet = self->v_mix;
			self->r.dry = 1.0 - self->v_mix;
		}
		if (iterpolate & 2) {
			self->v_roomsz += tau * ( *self->roomsz - self->v_roomsz);
			self->r.gain[0] = 0.773 * self->v_roomsz;
			self->r.gain[1] = 0.802 * self->v_roomsz;
			self->r.gain[2] = 0.753 * self->v_roomsz;
			self->r.gain[3] = 0.733 * self->v_roomsz;
		}

		reverb (&self->r,
				&input0[offset], &input1[offset],
				&output0[offset], &output1[offset],
				p_samples);

		offset += p_samples;
		remain -= p_samples;
	}
}


static void
cleanup (LV2_Handle instance)
{
	AReverb* self = (AReverb*)instance;
	for (int i = 0; i < RV_NZ; ++i) {
		free (self->r.delays[0][i]);
		free (self->r.delays[1][i]);
	}
	free (instance);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	"urn:ardour:a-reverb",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
