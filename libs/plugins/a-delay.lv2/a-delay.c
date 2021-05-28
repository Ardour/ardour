/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

#define ADELAY_URI "urn:ardour:a-delay"

// 8 seconds of delay at 96kHz
#define MAX_DELAY 768000

#ifndef M_PI
# define M_PI 3.1415926
#endif

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

typedef enum {
	ADELAY_INPUT = 0,
	ADELAY_OUTPUT,

	ADELAY_BPM,

	ADELAY_INV,
	ADELAY_SYNC,
	ADELAY_TIME,
	ADELAY_DIVISOR,
	ADELAY_WETDRY,
	ADELAY_FEEDBACK,
	ADELAY_LPF,
	ADELAY_GAIN,
	ADELAY_DELAYTIME,
	ADELAY_ENABLE,
	ADELAY_DOTTED,
} PortIndex;


typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_Sequence;
	LV2_URID atom_Long;
	LV2_URID atom_Int;
	LV2_URID atom_Float;
	LV2_URID atom_Double;
	LV2_URID time_beatUnit;
	LV2_URID time_beatsPerMinute;
	LV2_URID time_Position;
} DelayURIs;

typedef struct {
	float* input;
	float* output;

	const LV2_Atom_Sequence* atombpm;

	float* inv;
	float* dotted;
	float* sync;
	float* time;
	float* divisor;
	float* wetdry;
	float* feedback;
	float* lpf;
	float* gain;
	
	float* delaytime;
	float* enable;

	float srate;
	float bpm;
	float beatunit;
	int bpmvalid;

	uint32_t posz;
	float tap[2];
	float z[MAX_DELAY];
	int active;
	int next;
	float fbstate;
	float lpfold;
	float feedbackold;
	float divisorold;
	float gainold;
	float dottedold;
	float invertold;
	float timeold;
	float delaytimeold;
	float syncold;
	float wetdryold;
	float delaysamplesold;
	float tau;

	float A0, A1, A2, A3, A4, A5;
	float B0, B1, B2, B3, B4, B5;
	float state[4];

	DelayURIs uris;
	LV2_Atom_Forge forge;
	LV2_URID_Map* map;
} ADelay;

static inline void
map_uris(LV2_URID_Map* map, DelayURIs* uris)
{
	uris->atom_Blank          = map->map(map->handle, LV2_ATOM__Blank);
	uris->atom_Object         = map->map(map->handle, LV2_ATOM__Object);
	uris->atom_Sequence       = map->map(map->handle, LV2_ATOM__Sequence);
	uris->atom_Long           = map->map(map->handle, LV2_ATOM__Long);
	uris->atom_Int            = map->map(map->handle, LV2_ATOM__Int);
	uris->atom_Float          = map->map(map->handle, LV2_ATOM__Float);
	uris->atom_Double         = map->map(map->handle, LV2_ATOM__Double);
	uris->time_beatUnit       = map->map(map->handle, LV2_TIME__beatUnit);
	uris->time_beatsPerMinute = map->map(map->handle, LV2_TIME__beatsPerMinute);
	uris->time_Position       = map->map(map->handle, LV2_TIME__Position);
}

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	int i;
	ADelay* adelay = (ADelay*)calloc(1, sizeof(ADelay));
	if (!adelay) return NULL;

	for (i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID__map)) {
			adelay->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!adelay->map) {
		fprintf(stderr, "a-delay.lv2 error: Host does not support urid:map\n");
		free(adelay);
		return NULL;
	}

	map_uris(adelay->map, &adelay->uris);
	lv2_atom_forge_init(&adelay->forge, adelay->map);

	adelay->srate = rate;
	adelay->bpmvalid = 0;
	adelay->tau = (1.0 - exp (-2.f * M_PI * 25.f / adelay->srate));

	return (LV2_Handle)adelay;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	ADelay* adelay = (ADelay*)instance;

	switch ((PortIndex)port) {
	case ADELAY_INPUT:
		adelay->input = (float*)data;
		break;
	case ADELAY_OUTPUT:
		adelay->output = (float*)data;
		break;
	case ADELAY_BPM:
		adelay->atombpm = (const LV2_Atom_Sequence*)data;
		break;
	case ADELAY_INV:
		adelay->inv = (float*)data;
		break;
	case ADELAY_SYNC:
		adelay->sync = (float*)data;
		break;
	case ADELAY_TIME:
		adelay->time = (float*)data;
		break;
	case ADELAY_DIVISOR:
		adelay->divisor = (float*)data;
		break;
	case ADELAY_DOTTED:
		adelay->dotted = (float*)data;
		break;
	case ADELAY_WETDRY:
		adelay->wetdry = (float*)data;
		break;
	case ADELAY_FEEDBACK:
		adelay->feedback = (float*)data;
		break;
	case ADELAY_LPF:
		adelay->lpf = (float*)data;
		break;
	case ADELAY_GAIN:
		adelay->gain = (float*)data;
		break;
	case ADELAY_DELAYTIME:
		adelay->delaytime = (float*)data;
		break;
	case ADELAY_ENABLE:
		adelay->enable = (float*)data;
		break;
	}
}

static inline float
sanitize_denormal(const float value) {
	if (!isnormal(value)) {
		return 0.f;
	}
	return value;
}

static inline float
sanitize_input(const float value) {
	if (!isfinite_local (value)) {
		return 0.f;
	}
	return value;
}

static inline float
from_dB(float gdb) {
	return (exp(gdb/20.f*log(10.f)));
}

static inline bool
is_eq(float a, float b, float small) {
	return (fabsf(a - b) < small);
}

static void clearfilter(LV2_Handle instance)
{
	ADelay* adelay = (ADelay*)instance;

	adelay->state[0] = adelay->state[1] =
		adelay->state[2] = adelay->state[3] = 0.f;
}

static void
activate(LV2_Handle instance)
{
	ADelay* adelay = (ADelay*)instance;

	int i;
	for (i = 0; i < MAX_DELAY; i++) {
		adelay->z[i] = 0.f;
	}
	adelay->posz = 0;
	adelay->tap[0] = 0;
	adelay->tap[1] = 0;
	adelay->active = 0;
	adelay->next = 1;
	adelay->fbstate = 0.f;

	clearfilter(adelay);

	adelay->lpfold = 0.f;
	adelay->divisorold = 0.f;
	adelay->gainold = 0.f;
	adelay->invertold = 0.f;
	adelay->dottedold = 0.f;
	adelay->timeold = 0.f;
	adelay->delaytimeold = 0.f;
	adelay->syncold = 0.f;
	adelay->wetdryold = 0.f;
	adelay->delaysamplesold = 1.f;
}

static void lpfRbj(LV2_Handle instance, float fc, float srate)
{
	ADelay* adelay = (ADelay*)instance;

	float w0, alpha, cw, sw, q;
	q = 0.707;
	w0 = (2. * M_PI * fc / srate);
	sw = sin(w0);
	cw = cos(w0);
	alpha = sw / (2. * q);

	adelay->A0 = 1. + alpha;
	adelay->A1 = -2. * cw;
	adelay->A2 = 1. - alpha;
	adelay->B0 = (1. - cw) / 2.;
	adelay->B1 = (1. - cw);
	adelay->B2 = adelay->B0;

	adelay->A3 = 1. + alpha;
	adelay->A4 = -2. * cw;
	adelay->A5 = 1. - alpha;
	adelay->B3 = (1. - cw) / 2.;
	adelay->B4 = (1. - cw);
	adelay->B5 = adelay->B3;
}

static float runfilter(LV2_Handle instance, const float in)
{
	ADelay* a = (ADelay*)instance;

	float out;

	out = a->B0/a->A0*in + a->B1/a->A0*a->state[0] + a->B2/a->A0*a->state[1]
			-a->A1/a->A0*a->state[2] - a->A2/a->A0*a->state[3] + 1e-20;

	a->state[1] = a->state[0];
	a->state[0] = in;
	a->state[3] = a->state[2];
	a->state[2] = sanitize_input (out);
	return out;
}

static bool
update_bpm(ADelay* self, const LV2_Atom_Object* obj)
{
	bool changed = false;
	const DelayURIs* uris = &self->uris;

	// Received new transport bpm/beatunit
	LV2_Atom *beatunit = NULL, *bpm = NULL;
	lv2_atom_object_get(obj,
	                    uris->time_beatUnit, &beatunit,
	                    uris->time_beatsPerMinute, &bpm,
	                    NULL);
	// Tempo changed, update BPM
	if (bpm && bpm->type == uris->atom_Float) {
		float b = ((LV2_Atom_Float*)bpm)->body;
		if (self->bpm != b) {
			changed = true;
		}
		self->bpm = b;
	}
	// Time signature changed, update beatunit
	if (beatunit && beatunit->type == uris->atom_Int) {
		int b = ((LV2_Atom_Int*)beatunit)->body;
		if (self->beatunit != b) {
			changed = true;
		}
		self->beatunit = b;
	}
	self->bpmvalid = 1;
	return changed;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	ADelay* adelay = (ADelay*)instance;

	const float* const input = adelay->input;
	float* const output = adelay->output;

	const float srate = adelay->srate;
	const float tau = adelay->tau;

	float wetdry_target = *adelay->wetdry / 100.f;
	float gain_target = from_dB(*adelay->gain);
	float wetdry = adelay->wetdryold;
	float gain = adelay->gainold;

	if (*adelay->enable <= 0) {
		wetdry_target = 0.f;
		gain_target = 1.0;
	}

	uint32_t i;
	float in;
	int delaysamples = 0;
	unsigned int tmp;
	float inv;
	float xfade;
	bool recalc = false;

	// TODO LPF
	if (*(adelay->inv) < 0.5) {
		inv = -1.f;
	} else {
		inv = 1.f;
	}

	if (adelay->atombpm) {
		LV2_Atom_Event* ev = lv2_atom_sequence_begin(&(adelay->atombpm)->body);
		while(!lv2_atom_sequence_is_end(&(adelay->atombpm)->body, (adelay->atombpm)->atom.size, ev)) {
			if (ev->body.type == adelay->uris.atom_Blank || ev->body.type == adelay->uris.atom_Object) {
				const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
				if (obj->body.otype == adelay->uris.time_Position) {
					recalc = update_bpm(adelay, obj);
					// TODO: split process on BPM change. (independent of buffer-size)
				}
			}
			ev = lv2_atom_sequence_next(ev);
		}
	}

	if (*(adelay->inv) != adelay->invertold) {
		recalc = true;
	}
	if (*(adelay->dotted) != adelay->dottedold) {
		recalc = true;
	}
	if (*(adelay->sync) != adelay->syncold) {
		recalc = true;
	}
	if (*(adelay->time) != adelay->timeold) {
		recalc = true;
	}
	if (*(adelay->feedback) != adelay->feedbackold) {
		recalc = true;
	}
	if (*(adelay->divisor) != adelay->divisorold) {
		recalc = true;
	}
	if (!is_eq(adelay->lpfold, *adelay->lpf, 0.1)) {
		float  tc = (1.0 - exp (-2.f * M_PI * n_samples * 25.f / adelay->srate));
		adelay->lpfold += tc * (*adelay->lpf - adelay->lpfold);
		recalc = true;
	}
	
	// rg says: in case the delay-time changes, oversampling/interpolate + LPF
	// would me more appropriate.
	if (recalc) {
		lpfRbj(adelay, adelay->lpfold, srate);
		if (*(adelay->sync) > 0.5f && adelay->bpmvalid) {
			/* quarter notes / min : 4 qn * 1000 ms/s * 60 s/min = 24k */
			*(adelay->delaytime) = 240000.f / (adelay->bpm * *(adelay->divisor));
			if (*(adelay->dotted) > 0.5f) {
				*(adelay->delaytime) *= 1.5;
			}
		} else {
			*(adelay->delaytime) = *(adelay->time);
		}
		delaysamples = (int)(*(adelay->delaytime) * srate) / 1000;
		adelay->tap[adelay->next] = delaysamples;
	}

	xfade = 0.f;

	float fbstate = adelay->fbstate;
	const float feedback = *adelay->feedback;

	for (i = 0; i < n_samples; i++) {
		in = sanitize_input (input[i]);
		adelay->z[adelay->posz] = sanitize_denormal (in + feedback / 100.f * fbstate);
		int p = adelay->posz - adelay->tap[adelay->active]; // active line
		if (p<0) p += MAX_DELAY;
		fbstate = adelay->z[p];
		
		if (recalc) {
			xfade += 1.0f / (float)n_samples;
			fbstate *= (1.-xfade);
			p = adelay->posz - adelay->tap[adelay->next]; // next line
			if (p<0) p += MAX_DELAY;
			fbstate += adelay->z[p] * xfade;
		}

		wetdry += tau * (wetdry_target - wetdry) + 1e-12;
		gain += tau * (gain_target - gain) + 1e-12;

		output[i] = (1.f - wetdry) * in;
		output[i] += wetdry * -inv * runfilter(adelay, fbstate);
		output[i] *= gain;

		if (++(adelay->posz) >= MAX_DELAY) {
			adelay->posz = 0;
		}
	}

	adelay->fbstate = fbstate;
	adelay->feedbackold = *(adelay->feedback);
	adelay->divisorold = *(adelay->divisor);
	adelay->invertold = *(adelay->inv);
	adelay->dottedold = *(adelay->dotted);
	adelay->timeold = *(adelay->time);
	adelay->syncold = *(adelay->sync);
	adelay->wetdryold = wetdry;
	adelay->gainold = gain;
	adelay->delaytimeold = *(adelay->delaytime);
	adelay->delaysamplesold = delaysamples;

	if (recalc) {
		tmp = adelay->active;
		adelay->active = adelay->next;
		adelay->next = tmp;
	}

}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	ADELAY_URI,
	instantiate,
	connect_port,
	activate,
	run,
	NULL,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
