/*
 * Copyright (C) 2016-2017 Damien Zammit <damien@zamaudio.com>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

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
	AEQ_FREQL = 0,
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
	AEQ_FREQH,
	AEQ_GAINH,
	AEQ_MASTER,
	AEQ_FILTOGL,
	AEQ_FILTOG1,
	AEQ_FILTOG2,
	AEQ_FILTOG3,
	AEQ_FILTOG4,
	AEQ_FILTOGH,
	AEQ_ENABLE,
	AEQ_INPUT,
	AEQ_OUTPUT,
} PortIndex;

static inline double
to_dB(double g) {
	return (20.0*log10(g));
}

static inline double
from_dB(double gdb) {
	return (exp(gdb/20.0*log(10.0)));
}

static inline bool
is_eq(float a, float b, float small) {
	return (fabsf(a - b) < small);
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

static void linear_svf_protect(struct linear_svf *self)
{
	if (!isfinite_local (self->s[0]) || !isfinite_local (self->s[1])) {
		linear_svf_reset (self);
	}
}

typedef struct {
	float* f0[BANDS];
	float* g[BANDS];
	float* bw[BANDS];
	float* filtog[BANDS];
	float* master;
	float* enable;

	float srate;
	float tau;

	float* input;
	float* output;

	struct linear_svf v_filter[BANDS];
	float v_g[BANDS];
	float v_bw[BANDS];
	float v_f0[BANDS];
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
	Aeq* aeq = (Aeq*)calloc(1, sizeof(Aeq));
	aeq->srate = rate;
	aeq->tau = 1.f - expf (-2.f * M_PI * 64.f * 25.f / aeq->srate); // 25Hz time constant @ 64fpp

#ifdef LV2_EXTENDED
	for (int i=0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			aeq->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
	}
#endif

	for (int i = 0; i < BANDS; i++)
		linear_svf_reset(&aeq->v_filter[i]);

	aeq->need_expose = true;
#ifdef LV2_EXTENDED
	aeq->display = NULL;
#endif

	return (LV2_Handle)aeq;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	Aeq* aeq = (Aeq*)instance;

	switch ((PortIndex)port) {
	case AEQ_ENABLE:
		aeq->enable = (float*)data;
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

static void linear_svf_set_peq(struct linear_svf *self, float gdb, float sample_rate, float cutoff, float bandwidth)
{
	double f0 = (double)cutoff;
	double q = (double)pow(2.0, 0.5 * bandwidth) / (pow(2.0, bandwidth) - 1.0);
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

static void set_params(LV2_Handle instance, int band) {
	Aeq* aeq = (Aeq*)instance;

	switch (band) {
	case 0:
		linear_svf_set_lowshelf(&aeq->v_filter[0], aeq->v_g[0], aeq->srate, aeq->v_f0[0], 0.7071068);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		linear_svf_set_peq(&aeq->v_filter[band], aeq->v_g[band], aeq->srate, aeq->v_f0[band], aeq->v_bw[band]);
		break;
	case 5:
		linear_svf_set_highshelf(&aeq->v_filter[5], aeq->v_g[5], aeq->srate, aeq->v_f0[5], 0.7071068);
		break;
	}
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	Aeq* aeq = (Aeq*)instance;

	const float* const input = aeq->input;
	float* const output = aeq->output;

	const float tau = aeq->tau;
	uint32_t offset = 0;

	const float target_gain = *aeq->enable <= 0 ? 0 : *aeq->master; // dB

	while (n_samples > 0) {
		uint32_t block = n_samples;
		bool any_changed = false;

		if (!is_eq(aeq->v_master, target_gain, 0.1)) {
			aeq->v_master += tau * (target_gain - aeq->v_master);
			any_changed = true;
		} else {
			aeq->v_master = target_gain;
		}

		for (int i = 0; i < BANDS; ++i) {
			bool changed = false;

			if (!is_eq(aeq->v_f0[i], *aeq->f0[i], 0.1)) {
				aeq->v_f0[i] += tau * (*aeq->f0[i] - aeq->v_f0[i]);
				changed = true;
			}

			if (*aeq->filtog[i] <= 0 || *aeq->enable <= 0) {
				if (!is_eq(aeq->v_g[i], 0.f, 0.05)) {
					aeq->v_g[i] += tau * (0.0 - aeq->v_g[i]);
					changed = true;
				}
			} else {
				if (!is_eq(aeq->v_g[i], *aeq->g[i], 0.05)) {
					aeq->v_g[i] += tau * (*aeq->g[i] - aeq->v_g[i]);
					changed = true;
				}
			}

			if (i != 0 && i != 5) {
				if (!is_eq(aeq->v_bw[i], *aeq->bw[i], 0.001)) {
					aeq->v_bw[i] += tau * (*aeq->bw[i] - aeq->v_bw[i]);
					changed = true;
				}
			}

			if (changed) {
				set_params(aeq, i);
				any_changed = true;
			}
		}

		if (any_changed) {
			aeq->need_expose = true;
			block = MIN (64, n_samples);
		}

		for (uint32_t i = 0; i < block; ++i) {
			float in0, out;
			in0 = input[i + offset];
			out = in0;
			for (uint32_t j = 0; j < BANDS; j++) {
				out = run_linear_svf(&aeq->v_filter[j], out);
			}
			output[i + offset] = out * from_dB(aeq->v_master);
		}
		n_samples -= block;
		offset += block;
	}

	for (uint32_t j = 0; j < BANDS; j++) {
		linear_svf_protect(&aeq->v_filter[j]);
	}

#ifdef LV2_EXTENDED
	if (aeq->need_expose && aeq->queue_draw) {
		aeq->need_expose = false;
		aeq->queue_draw->queue_draw (aeq->queue_draw->handle);
	}
#endif
}

static double
calc_peq(Aeq* self, int i, double omega) {
	double complex H = 0.0;
	double complex z = cexp(I * omega);
	double complex zz = cexp(2. * I * omega);
	double complex zm = z - 1.0;
	double complex zp = z + 1.0;
	double complex zzm = zz - 1.0;

	double A = pow(10.0, self->v_g[i]/40.0);
	double g = self->v_filter[i].g;
	double k = self->v_filter[i].k * A;
	double m1 = k * (A * A - 1.0) / A;

	H = (g*k*zzm + A*(g*zp*(m1*zm) + (zm*zm + g*g*zp*zp))) / (g*k*zzm + A*(zm*zm + g*g*zp*zp));
	return cabs(H);
}

static double
calc_lowshelf(Aeq* self, double omega) {
	double complex H = 0.0;
	double complex z = cexp(I * omega);
	double complex zz = cexp(2. * I * omega);
	double complex zm = z - 1.0;
	double complex zp = z + 1.0;
	double complex zzm = zz - 1.0;

	double A = pow(10.0, self->v_g[0]/40.0);
	double g = self->v_filter[0].g;
	double k = self->v_filter[0].k;
	double m0 = self->v_filter[0].m[0];
	double m1 = self->v_filter[0].m[1];
	double m2 = self->v_filter[0].m[2];

	H = (A*m0*zm*zm + g*g*(m0+m2)*zp*zp + sqrt(A)*g*(k*m0+m1) * zzm) / (A*zm*zm + g*g*zp*zp + sqrt(A)*g*k*zzm);
	return cabs(H);
}

static double
calc_highshelf(Aeq* self, double omega) {
	double complex H = 0.0;
	double complex z = cexp(I * omega);
	double complex zz = cexp(2. * I * omega);
	double complex zm = z - 1.0;
	double complex zp = z + 1.0;
	double complex zzm = zz - 1.0;

	double A = pow(10.0, self->v_g[5]/40.0);
	double g = self->v_filter[5].g;
	double k = self->v_filter[5].k;
	double m0 = self->v_filter[5].m[0];
	double m1 = self->v_filter[5].m[1];
	double m2 = self->v_filter[5].m[2];

	H = ( sqrt(A) * g * zp * (m1 * zm + sqrt(A)*g*m2*zp) + m0 * ( zm*zm + A*g*g*zp*zp + sqrt(A)*g*k*zzm)) / (zm*zm + A*g*g*zp*zp + sqrt(A)*g*k*zzm);
	return cabs(H);
}

#ifdef LV2_EXTENDED
static float
eq_curve (Aeq* self, float f) {
	double response = 1.0;
	double SR = (double)self->srate;
	double omega = f * 2. * M_PI / SR;

	// lowshelf
	response *= calc_lowshelf(self, omega);

	// peq 1 - 4:
	response *= calc_peq(self, 1, omega);
	response *= calc_peq(self, 2, omega);
	response *= calc_peq(self, 3, omega);
	response *= calc_peq(self, 4, omega);

	// highshelf:
	response *= calc_highshelf(self, omega);

	return (float)response;
}

static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	Aeq* self = (Aeq*)instance;
	uint32_t h = MIN (1 | (uint32_t)ceilf (w * 9.f / 16.f), max_h);

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

	// prepare grid drawing
	cairo_save (cr);
	const double dash2[] = {1, 3};
	//cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	// draw x-grid 6dB steps
	for (int32_t d = -18; d <= 18; d+=6) {
		float y = (float)h * (d / 40.0 + 0.5);
		y = rint (y) - .5;
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	// draw y-axis grid 100, 1k, 10K
	for (int32_t f = 100; f <= 10000; f *= 10) {
		float x = w * log10 (f / 20.0) / log10 (1000.0);
		x = rint (x) - .5;
		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
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
		const float y = (float)h * (-y_db / 40.0 + 0.5);
		cairo_line_to (cr, x, y);
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
