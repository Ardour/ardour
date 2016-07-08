/* a-eq
 * Copyright (C) 2016 Damien Zammit <damien@zamaudio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for M_PI
#endif

#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

#define AEQ_URI	"urn:ardour:a-eq"
#define BANDS	6

#ifndef MIN
#define MIN(A,B) ((A) < (B)) ? (A) : (B)
#endif

typedef enum {
	AEQ_SHELFTOGL = 0,
	AEQ_FREQL,
	AEQ_GAINL,
	AEQ_FREQ1,
	AEQ_GAIN1,
	AEQ_BW1,
	AEQ_FREQ2,
	AEQ_GAIN2,
	AEQ_BW2,
	AEQ_FREQ3,
	AEQ_GAIN3,
	AEQ_BW3,
	AEQ_FREQ4,
	AEQ_GAIN4,
	AEQ_BW4,
	AEQ_SHELFTOGH,
	AEQ_FREQH,
	AEQ_GAINH,
	AEQ_MASTER,
	AEQ_FILTOGL,
	AEQ_FILTOG1,
	AEQ_FILTOG2,
	AEQ_FILTOG3,
	AEQ_FILTOG4,
	AEQ_FILTOGH,
	AEQ_INPUT,
	AEQ_OUTPUT,
} PortIndex;

static inline float
to_dB(float g) {
	return (20.f*log10(g));
}

static inline float
from_dB(float gdb) {
	return (exp(gdb/20.f*log(10.f)));
}

struct linear_svf {
	double g, k;
	double a[3];
	double m[3];
	double s[2];
};

static void linear_svf_reset(struct linear_svf *self)
{
	self->s[0] = self->s[1] = 0.0;
}

typedef struct {
	float* shelftogl;
	float* shelftogh;
	float* f0[BANDS];
	float* g[BANDS];
	float* bw[BANDS];
	float* filtog[BANDS];
	float* master;

	float srate;

	float* input;
	float* output;

	struct linear_svf v_filter[BANDS];
	float v_g[BANDS];
	float v_bw[BANDS];
	float v_f0[BANDS];
	float v_filtog[BANDS];
	float v_shelftogl;
	float v_shelftogh;
	float v_master;

	bool need_expose;

#ifdef LV2_EXTENDED
	LV2_Inline_Display_Image_Surface surf;
	cairo_surface_t*                 display;
	LV2_Inline_Display*              queue_draw;
	uint32_t                         w, h;
#endif
} Aeq;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	int i;
	Aeq* aeq = (Aeq*)malloc(sizeof(Aeq));
	aeq->srate = rate;
	
#ifdef LV2_EXTENDED
	for (int i=0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			aeq->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
	}
#endif

	for (i = 0; i < BANDS; i++)
		linear_svf_reset(&aeq->v_filter[i]);

	aeq->need_expose = true;
	aeq->display = NULL;

	return (LV2_Handle)aeq;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	Aeq* aeq = (Aeq*)instance;

	switch ((PortIndex)port) {
	case AEQ_SHELFTOGL:
		aeq->shelftogl = (float*)data;
		break;
	case AEQ_FREQL:
		aeq->f0[0] = (float*)data;
		break;
	case AEQ_GAINL:
		aeq->g[0] = (float*)data;
		break;
	case AEQ_FREQ1:
		aeq->f0[1] = (float*)data;
		break;
	case AEQ_GAIN1:
		aeq->g[1] = (float*)data;
		break;
	case AEQ_BW1:
		aeq->bw[1] = (float*)data;
		break;
	case AEQ_FREQ2:
		aeq->f0[2] = (float*)data;
		break;
	case AEQ_GAIN2:
		aeq->g[2] = (float*)data;
		break;
	case AEQ_BW2:
		aeq->bw[2] = (float*)data;
		break;
	case AEQ_FREQ3:
		aeq->f0[3] = (float*)data;
		break;
	case AEQ_GAIN3:
		aeq->g[3] = (float*)data;
		break;
	case AEQ_BW3:
		aeq->bw[3] = (float*)data;
		break;
	case AEQ_FREQ4:
		aeq->f0[4] = (float*)data;
		break;
	case AEQ_GAIN4:
		aeq->g[4] = (float*)data;
		break;
	case AEQ_BW4:
		aeq->bw[4] = (float*)data;
		break;
	case AEQ_SHELFTOGH:
		aeq->shelftogh = (float*)data;
		break;
	case AEQ_FREQH:
		aeq->f0[5] = (float*)data;
		break;
	case AEQ_GAINH:
		aeq->g[5] = (float*)data;
		break;
	case AEQ_MASTER:
		aeq->master = (float*)data;
		break;
	case AEQ_FILTOGL:
		aeq->filtog[0] = (float*)data;
		break;
	case AEQ_FILTOG1:
		aeq->filtog[1] = (float*)data;
		break;
	case AEQ_FILTOG2:
		aeq->filtog[2] = (float*)data;
		break;
	case AEQ_FILTOG3:
		aeq->filtog[3] = (float*)data;
		break;
	case AEQ_FILTOG4:
		aeq->filtog[4] = (float*)data;
		break;
	case AEQ_FILTOGH:
		aeq->filtog[5] = (float*)data;
		break;
	case AEQ_INPUT:
		aeq->input = (float*)data;
		break;
	case AEQ_OUTPUT:
		aeq->output = (float*)data;
		break;
	}
}

static void
activate(LV2_Handle instance)
{
	int i;
	Aeq* aeq = (Aeq*)instance;

	for (i = 0; i < BANDS; i++)
		linear_svf_reset(&aeq->v_filter[i]);
}

// SVF filters
// http://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

static void linear_svf_set_hp(struct linear_svf *self, float sample_rate, float cutoff, float resonance)
{
	double f0 = (double)cutoff;
	double q = (double)resonance;
	double sr = (double)sample_rate;

	self->g = tan(M_PI * (f0 / sr));
	self->k = 1.0 / q;

	self->a[0] = 1.0 / (1.0 + self->g * (self->g + self->k));
	self->a[1] = self->g * self->a[0];
	self->a[2] = self->g * self->a[1];

	self->m[0] = 1.0;
	self->m[1] = -self->k;
	self->m[2] = -1.0;
}

static void linear_svf_set_lp(struct linear_svf *self, float sample_rate, float cutoff, float resonance)
{
	double f0 = (double)cutoff;
	double q = (double)resonance;
	double sr = (double)sample_rate;

	self->g = tan(M_PI * (f0 / sr));
	self->k = 1.0 / q;

	self->a[0] = 1.0 / (1.0 + self->g * (self->g + self->k));
	self->a[1] = self->g * self->a[0];
	self->a[2] = self->g * self->a[1];

	self->m[0] = 0.0;
	self->m[1] = 0.0;
	self->m[2] = 1.0;
}

static void linear_svf_set_peq(struct linear_svf *self, float gdb, float sample_rate, float cutoff, float bandwidth)
{
	double f0 = (double)cutoff;
	double q = (double)pow(2.0, 1.0 / bandwidth) / (pow(2.0, bandwidth) - 1.0);
	double sr = (double)sample_rate;
	double A = pow(10.0, gdb/40.0);

	self->g = tan(M_PI * (f0 / sr));
	self->k = 1.0 / (q * A);

	self->a[0] = 1.0 / (1.0 + self->g * (self->g + self->k));
	self->a[1] = self->g * self->a[0];
	self->a[2] = self->g * self->a[1];

	self->m[0] = 1.0;
	self->m[1] = self->k * (A * A - 1.0);
	self->m[2] = 0.0;
}

static void linear_svf_set_highshelf(struct linear_svf *self, float gdb, float sample_rate, float cutoff, float resonance)
{
	double f0 = (double)cutoff;
	double q = (double)resonance;
	double sr = (double)sample_rate;
	double A = pow(10.0, gdb/40.0);

	self->g = tan(M_PI * (f0 / sr));
	self->k = 1.0 / q;

	self->a[0] = 1.0 / (1.0 + self->g * (self->g + self->k));
	self->a[1] = self->g * self->a[0];
	self->a[2] = self->g * self->a[1];

	self->m[0] = A * A;
	self->m[1] = self->k * (1.0 - A) * A;
	self->m[2] = 1.0 - A * A;
}

static void linear_svf_set_lowshelf(struct linear_svf *self, float gdb, float sample_rate, float cutoff, float resonance)
{
	double f0 = (double)cutoff;
	double q = (double)resonance;
	double sr = (double)sample_rate;
	double A = pow(10.0, gdb/40.0);

	self->g = tan(M_PI * (f0 / sr));
	self->k = 1.0 / q;

	self->a[0] = 1.0 / (1.0 + self->g * (self->g + self->k));
	self->a[1] = self->g * self->a[0];
	self->a[2] = self->g * self->a[1];

	self->m[0] = 1.0;
	self->m[1] = self->k * (A - 1.0);
	self->m[2] = A * A - 1.0;
}

static float run_linear_svf(struct linear_svf *self, float in)
{
	double v[3];
	double din = (double)in;
	double out;

	v[2] = din - self->s[1];
	v[0] = (self->a[0] * self->s[0]) + (self->a[1] * v[2]);
	v[1] = self->s[1] + (self->a[1] * self->s[0]) + (self->a[2] * v[2]);

	self->s[0] = (2.0 * v[0]) - self->s[0];
	self->s[1] = (2.0 * v[1]) - self->s[1];

	out = (self->m[0] * din)
		+ (self->m[1] * v[0])
		+ (self->m[2] * v[1]);

	return (float)out;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	Aeq* aeq = (Aeq*)instance;

	const float* const input = aeq->input;
	float* const output = aeq->output;

	float srate = aeq->srate;
	float in0, out;
	uint32_t i, j;

	if (*(aeq->shelftogl) > 0.5) {
		linear_svf_set_lowshelf(&aeq->v_filter[0], *(aeq->g[0]), srate, *(aeq->f0[0]), 0.7071068);
	} else {
		linear_svf_set_hp(&aeq->v_filter[0], srate, *(aeq->f0[0]), 0.7071068);
	}
	linear_svf_set_peq(&aeq->v_filter[1], *(aeq->g[1]), srate, *(aeq->f0[1]), *(aeq->bw[1]));
	linear_svf_set_peq(&aeq->v_filter[2], *(aeq->g[2]), srate, *(aeq->f0[2]), *(aeq->bw[2]));
	linear_svf_set_peq(&aeq->v_filter[3], *(aeq->g[3]), srate, *(aeq->f0[3]), *(aeq->bw[3]));
	linear_svf_set_peq(&aeq->v_filter[4], *(aeq->g[4]), srate, *(aeq->f0[4]), *(aeq->bw[4]));

	if (*(aeq->shelftogh) > 0.5) {
		linear_svf_set_highshelf(&aeq->v_filter[5], *(aeq->g[5]), srate, *(aeq->f0[5]), 0.7071068);
	} else {
		linear_svf_set_lp(&aeq->v_filter[5], srate, *(aeq->f0[5]), 0.7071068);
	}

	for (i = 0; i < n_samples; i++) {
		in0 = input[i];
		out = in0;
		for (j = 0; j < BANDS; j++) {
			if (*(aeq->filtog[j]) > 0.5)
				out = run_linear_svf(&aeq->v_filter[j], out);
		}
		output[i] = out * from_dB(*(aeq->master));
	}

	for (i = 0; i < BANDS; i++) {
		if (aeq->v_f0[i] != *(aeq->f0[i])) {
			aeq->v_f0[i] = *(aeq->f0[i]);
			aeq->need_expose = true;
		}
		if (aeq->v_g[i] != *(aeq->g[i])) {
			aeq->v_g[i] = *(aeq->g[i]);
			aeq->need_expose = true;
		}
		if (i != 0 && i != 5 && aeq->v_bw[i] != *(aeq->bw[i])) {
			aeq->v_bw[i] = *(aeq->bw[i]);
			aeq->need_expose = true;
		}
		if (aeq->v_filtog[i] != *(aeq->filtog[i])) {
			aeq->v_filtog[i] = *(aeq->filtog[i]);
			aeq->need_expose = true;
		}
		if (aeq->v_shelftogl != *(aeq->shelftogl)) {
			aeq->v_shelftogl = *(aeq->shelftogl);
			aeq->need_expose = true;
		}
		if (aeq->v_shelftogh != *(aeq->shelftogh)) {
			aeq->v_shelftogh = *(aeq->shelftogh);
			aeq->need_expose = true;
		}
		if (aeq->v_master != *(aeq->master)) {
			aeq->v_master = *(aeq->master);
			aeq->need_expose = true;
		}
	}

#ifdef LV2_EXTENDED
	if (aeq->need_expose && aeq->queue_draw) {
		aeq->need_expose = false;
		aeq->queue_draw->queue_draw (aeq->queue_draw->handle);
	}
#endif
}


#ifdef LV2_EXTENDED
static float
eq_curve (Aeq* self, float f) {
	float SR = self->srate;
	double complex H = 1.0;
	double theta = f * 2. * M_PI / SR;
	double complex z = cexp(I * theta);
	double A;
	double m0, m1, m2, g, k;
	int j = 0;

	// low
	if (self->v_filtog[0]) {
		A = pow(10.0, self->v_g[0]/40.0);
		m0 = self->v_filter[0].m[0];
		m1 = self->v_filter[0].m[1];
		m2 = self->v_filter[0].m[2];
		g = self->v_filter[0].g;
		k = self->v_filter[0].k;
		if (self->v_shelftogl) {
			// lowshelf
			H *= (A*m0*(z-1.0)*(z-1.0) + g*g*(m0+m2)*(1.0+z)*(1.0+z) + sqrt(A)*g*(k*m0+m1) * (z*z-1.0)) / (A*(z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z) + sqrt(A)*g*k*(z*z-1.0));
		} else {
			// hp:
			H *= ((z-1.0)*(z-1.0)) / ((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z) + g*k*(z*z-1.0));
		}
		j++;
	}

	// peq1:
	if (self->v_filtog[1]) {
		A = pow(10.0, self->v_g[1]/40.0);
		m0 = self->v_filter[1].m[0];
		m1 = self->v_filter[1].m[1];
		m2 = self->v_filter[1].m[2];
		g = self->v_filter[1].g;
		k = self->v_filter[1].k;
		H *= (g*k*m0*(z*z-1.0) + A*(g*(1.0+z)*(m1*(z-1.0) + g*m2*(1.0+z)) + m0*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)))) / (g*k*(z*z-1.0) + A*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)));
		j++;
	}

	// peq2:
	if (self->v_filtog[2]) {
		A = pow(10.0, self->v_g[2]/40.0);
		m0 = self->v_filter[2].m[0];
		m1 = self->v_filter[2].m[1];
		m2 = self->v_filter[2].m[2];
		g = self->v_filter[2].g;
		k = self->v_filter[2].k;
		H *= (g*k*m0*(z*z-1.0) + A*(g*(1.0+z)*(m1*(z-1.0) + g*m2*(1.0+z)) + m0*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)))) / (g*k*(z*z-1.0) + A*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)));
		j++;
	}

	// peq3:
	if (self->v_filtog[3]) {
		A = pow(10.0, self->v_g[3]/40.0);
		m0 = self->v_filter[3].m[0];
		m1 = self->v_filter[3].m[1];
		m2 = self->v_filter[3].m[2];
		g = self->v_filter[3].g;
		k = self->v_filter[3].k;
		H *= (g*k*m0*(z*z-1.0) + A*(g*(1.0+z)*(m1*(z-1.0) + g*m2*(1.0+z)) + m0*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)))) / (g*k*(z*z-1.0) + A*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)));
		j++;
	}

	// peq4:
	if (self->v_filtog[4]) {
		A = pow(10.0, self->v_g[4]/40.0);
		m0 = self->v_filter[4].m[0];
		m1 = self->v_filter[4].m[1];
		m2 = self->v_filter[4].m[2];
		g = self->v_filter[4].g;
		k = self->v_filter[4].k;
		H *= (g*k*m0*(z*z-1.0) + A*(g*(1.0+z)*(m1*(z-1.0) + g*m2*(1.0+z)) + m0*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)))) / (g*k*(z*z-1.0) + A*((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z)));
		j++;
	}

	// high
	if (self->v_filtog[5]) {
		A = pow(10.0, self->v_g[5]/40.0);
		m0 = self->v_filter[5].m[0];
		m1 = self->v_filter[5].m[1];
		m2 = self->v_filter[5].m[2];
		g = self->v_filter[5].g;
		k = self->v_filter[5].k;
		if (self->v_shelftogh) {
			// highshelf:
			H *= ( sqrt(A) * g * (1.0 + z) * (m1 * (z - 1.0) + sqrt(A)*g*m2*(1.0+z)) + m0 * ( (z-1.0)*(z-1.0) + A*g*g*(1.0+z)*(1.0+z) + sqrt(A)*g*k*(z*z-1.0))) / ((z-1.0)*(z-1.0) + A*g*g*(1.0+z)*(1.0+z) + sqrt(A)*g*k*(z*z-1.0));
		} else {
			// lp:
			H *= (g*g*(1.0+z)*(1.0+z)) / ((z-1.0)*(z-1.0) + g*g*(1.0+z)*(1.0+z) + g*k*(z*z-1.0));
		}
		j++;
	}

	return cabs(H);
}

static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	Aeq* self = (Aeq*)instance;
	uint32_t h = MIN (w, max_h);

	if (!self->display || self->w != w || self->h != h) {
		if (self->display) cairo_surface_destroy(self->display);
		self->display = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		self->w = w;
		self->h = h;
	}

	cairo_t* cr = cairo_create (self->display);

	// clear background
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);

	cairo_set_line_width(cr, 1.0);

	// draw grid 5dB steps
	const double dash2[] = {1, 3};
	cairo_save (cr);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	for (uint32_t d = 1; d < 8; ++d) {
		const float y = -.5 + floorf (h * (d * 5.f / 40.f));
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	cairo_restore (cr);


	// draw curve
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
	cairo_move_to (cr, 0, h);

	for (uint32_t x = 0; x < w; ++x) {
		// plot 20..20kHz +-20dB
		const float x_hz = 20.f * powf (1000.f, (float)x / (float)w);
		const float y_db = to_dB(eq_curve(self, x_hz)) + self->v_master;
		const float y = h * -y_db / 40.0 + h / 2;
		cairo_line_to (cr, x, y);
		//printf("(hz,H,db)=(%f, %f, %f)\n", x_hz, from_dB(y_db), y_db);
	}
	cairo_stroke_preserve (cr);

	cairo_line_to (cr, w, h);
	cairo_close_path (cr);
	cairo_clip (cr);

	// create RGBA surface
	cairo_destroy (cr);
	cairo_surface_flush (self->display);
	self->surf.width = cairo_image_surface_get_width (self->display);
	self->surf.height = cairo_image_surface_get_height (self->display);
	self->surf.stride = cairo_image_surface_get_stride (self->display);
	self->surf.data = cairo_image_surface_get_data  (self->display);

	return &self->surf;
}
#endif

static const void*
extension_data(const char* uri)
{
#ifdef LV2_EXTENDED
	static const LV2_Inline_Display_Interface display  = { render_inline };
	if (!strcmp(uri, LV2_INLINEDISPLAY__interface)) {
		return &display;
	}
#endif
	return NULL;
}

static void
cleanup(LV2_Handle instance)
{
#ifdef LV2_EXTENDED
	Aeq* aeq = (Aeq*)instance;
	if (aeq->display) {
		cairo_surface_destroy (aeq->display);
	}
#endif
	free(instance);
}

static const LV2_Descriptor descriptor = {
	AEQ_URI,
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
