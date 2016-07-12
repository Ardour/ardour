/* a-reverb -- based on b_reverb (setBfree)
 *
 * Copyright (C) 2003-2004 Fredrik Kilander <fk@dsv.su.se>
 * Copyright (C) 2008-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Will Panther <pantherb@setbfree.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#define RV_NZ 7
#define DENORMAL_PROTECT (1e-14)

typedef struct {
	float* delays[RV_NZ]; /**< delay line buffer */

	float* idx0[RV_NZ];	/**< Reset pointer ref delays[]*/
	float* idxp[RV_NZ];	/**< Index pointer ref delays[]*/
	float* endp[RV_NZ];	/**< End pointer   ref delays[]*/

	float gain[RV_NZ]; /**< feedback gains */
	float yy1; /**< Previous output sample */
	float y_1; /**< Feedback sample */

	int end[RV_NZ];

	float inputGain;	/**< Input gain value */
	float fbk;	/**< Feedback gain */
	float wet;	/**< Output dry gain */
	float dry;	/**< Output wet gain */
} b_reverb;

static int
setReverbPointers (b_reverb *r, int i, const double rate)
{
	int e = (r->end[i] * rate / 25000.0);
	e = e | 1;
	r->delays[i] = (float*)realloc ((void*)r->delays[i], (e + 2) * sizeof (float));
	if (!r->delays[i]) {
		return -1;
	} else {
		memset (r->delays[i], 0 , (e + 2) * sizeof (float));
	}
	r->endp[i] = r->delays[i] + e + 1;
	r->idx0[i] = r->idxp[i] = &(r->delays[i][0]);

	return 0;
}

static int
initReverb (b_reverb *r, const double rate)
{
	int err = 0;
	r->inputGain = 0.1; /* Input gain value */
	r->fbk = -0.015; /* Feedback gain */
	r->wet = 0.1; /* Output dry gain */
	r->dry = 0.9; /* Output wet gain */

	/* feedback combfilter */
	r->gain[0] = 0.773;
	r->gain[1] = 0.802;
	r->gain[2] = 0.753;
	r->gain[3] = 0.733;

	/* all-pass filter */
	r->gain[4] = sqrtf (0.5);
	r->gain[5] = sqrtf (0.5);
	r->gain[6] = sqrtf (0.5);

	/* delay lines */
	r->end[0] = 1687;
	r->end[1] = 1601;
	r->end[2] = 2053;
	r->end[3] = 2251;

	/* all pass filters */
	r->end[4] = 347;
	r->end[5] = 113;
	r->end[6] = 37;

	for (int i = 0; i < RV_NZ; ++i) {
		r->delays[i]= NULL;
	}

	r->yy1 = 0.0;
	r->y_1 = 0.0;

	for (int i = 0; i < RV_NZ; i++) {
		err |= setReverbPointers (r, i, rate);
	}
	return err;
}

static void
reverb (b_reverb* r,
        const float* inbuf,
        float* outbuf,
        size_t n_samples)
{
	float** const idxp = r->idxp;
	float* const* const endp = r->endp;
	float* const* const idx0 = r->idx0;
	const float* const gain = r->gain;
	const float inputGain = r->inputGain;
	const float fbk = r->fbk;
	const float wet = r->wet;
	const float dry = r->dry;

	const float* xp = inbuf;
	float* yp = outbuf;

	float y_1 = r->y_1;
	float yy1 = r->yy1;

	for (size_t i = 0; i < n_samples; ++i) {
		int j;
		float y;
		const float xo = *xp++;
		const float x = y_1 + (inputGain * xo);
		float xa = 0.0;
		/* First we do four feedback comb filters (ie parallel delay lines,
		 * each with a single tap at the end that feeds back at the start) */

		for (j = 0; j < 4; ++j) {
			y = *idxp[j];
			*idxp[j] = x + (gain[j] * y);
			if (endp[j] <= ++(idxp[j])) {
				idxp[j] = idx0[j];
			}
			xa += y;
		}

		for (; j < 7; ++j) {
			y = *idxp[j];
			*idxp[j] = gain[j] * (xa + y);
			if (endp[j] <= ++(idxp[j])) {
				idxp[j] = idx0[j];
			}
			xa = y - xa;
		}

		y = 0.5f * (xa + yy1);
		yy1 = y;
		y_1 = fbk * xa;

		*yp++ = ((wet * y) + (dry * xo));
	}

	r->y_1 = y_1 + DENORMAL_PROTECT;
	r->yy1 = yy1 + DENORMAL_PROTECT;
}

/******************************************************************************
 * LV2 wrapper
 */

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

typedef enum {
	AR_INPUT      = 0,
	AR_OUTPUT     = 1,
	AR_MIX        = 2,
	AR_GAIN_IN    = 3,
	AR_GAIN_OUT   = 4,
} PortIndex;

typedef struct {
	float* input;
	float* output;

	float* mix;
	float* gain_in;
	float* gain_out; // unused

	float v_mix;
	float v_gain_in;

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
	self->v_gain_in = -40; // [dB]
	self->v_mix = 0.1;

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	AReverb* self = (AReverb*)instance;

	switch ((PortIndex)port) {
		case AR_INPUT:
			self->input = (float*)data;
			break;
		case AR_OUTPUT:
			self->output = (float*)data;
			break;
		case AR_MIX:
			self->mix = (float*)data;
			break;
		case AR_GAIN_IN:
			self->gain_in = (float*)data;
			break;
		case AR_GAIN_OUT:
			self->gain_out = (float*)data;
			break;
	}
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	AReverb* self = (AReverb*)instance;

	const float* const input  = self->input;
	float* const       output = self->output;

	// TODO interpolate
	if (*self->mix != self->v_mix) {
		self->v_mix = *self->mix;
		const float u = self->r.wet + self->r.dry;
		self->r.wet = self->v_mix * u;
		self->r.dry = u - (self->v_mix * u);
	}
	if (*self->gain_in != self->v_gain_in) {
		self->v_gain_in = *self->gain_in;
		self->r.inputGain = powf (10.0, .05 * self->v_gain_in);
	}
	if (self->gain_out) { // unused
		const float g = *self->gain_out;
		const float u = self->r.wet + self->r.dry;
		self->r.wet = g * (self->r.wet / u);
		self->r.dry = g * (self->r.dry / u);
	}

	reverb (&self->r, input, output, n_samples);
}

static void
activate (LV2_Handle instance)
{
	AReverb* self = (AReverb*)instance;
	self->r.y_1 = 0;
	self->r.yy1 = 0;
}

static void
deactivate (LV2_Handle instance)
{
}

static void
cleanup (LV2_Handle instance)
{
	AReverb* self = (AReverb*)instance;
	for (int i = 0; i < RV_NZ; ++i) {
		free (self->r.delays[i]);
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
