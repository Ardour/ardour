/*
 * Copyright (C) 2016-2017 Damien Zammit <damien@zamaudio.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2019 Johannes Mueller <github@johannes-mueller.org>
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
#include <string.h>
#include <stdbool.h>

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ACOMP_URI		"urn:ardour:a-comp"
#define ACOMP_STEREO_URI	"urn:ardour:a-comp#stereo"

#define RESET_PEAK_AFTER_SECONDS 3

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#define MINUS_60 0.0001f

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

#ifndef FLT_EPSILON
#  define FLT_EPSILON 1.192093e-07
#endif

typedef enum {
	ACOMP_ATTACK = 0,
	ACOMP_RELEASE,
	ACOMP_KNEE,
	ACOMP_RATIO,
	ACOMP_THRESHOLD,
	ACOMP_MAKEUP,

	ACOMP_GAINR,
	ACOMP_INLEVEL,
	ACOMP_OUTLEVEL,
	ACOMP_SIDECHAIN,
	ACOMP_ENABLE,

	ACOMP_A0,
	ACOMP_A1,
	ACOMP_A2,
	ACOMP_A3,
	ACOMP_A4,
} PortIndex;

typedef struct {
	float* attack;
	float* release;
	float* knee;
	float* ratio;
	float* thresdb;
	float* makeup;

	float* gainr;
	float* outlevel;
	float* inlevel;
	float* sidechain;
	float* enable;

	float* input0;
	float* input1;
	float* sc;
	float* output0;
	float* output1;

	uint32_t n_channels;

	float srate;

	float makeup_gain;

#ifdef LV2_EXTENDED
	LV2_Inline_Display_Image_Surface surf;
	bool                     need_expose;
	cairo_surface_t*         display;
	LV2_Inline_Display*      queue_draw;
	uint32_t                 w, h;

	/* ports pointers are only valid during run so we'll
	 * have to cache them for the display, besides
	 * we do want to check for changes
	 */
	float v_knee;
	float v_ratio;
	float v_thresdb;
	float v_gainr;
	float v_makeup;
	float v_lvl_in;
	float v_lvl_out;
	float v_state_x;

	float v_peakdb;
	uint32_t peakdb_samples;
#endif
} AComp;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	AComp* acomp = (AComp*)calloc(1, sizeof(AComp));

	if (!strcmp (descriptor->URI, ACOMP_URI)) {
		acomp->n_channels = 1;
	} else if (!strcmp (descriptor->URI, ACOMP_STEREO_URI)) {
		acomp->n_channels = 2;
	} else {
		free (acomp);
		return NULL;
	}

	for (int i=0; features[i]; ++i) {
#ifdef LV2_EXTENDED
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			acomp->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
#endif
	}

	acomp->srate = rate;
	acomp->makeup_gain = 1.f;
#ifdef LV2_EXTENDED
	acomp->need_expose = true;
	acomp->v_lvl_out = -70.f;
#endif

	return (LV2_Handle)acomp;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	AComp* acomp = (AComp*)instance;

	switch ((PortIndex)port) {
		case ACOMP_ATTACK:
			acomp->attack = (float*)data;
			break;
		case ACOMP_RELEASE:
			acomp->release = (float*)data;
			break;
		case ACOMP_KNEE:
			acomp->knee = (float*)data;
			break;
		case ACOMP_RATIO:
			acomp->ratio = (float*)data;
			break;
		case ACOMP_THRESHOLD:
			acomp->thresdb = (float*)data;
			break;
		case ACOMP_MAKEUP:
			acomp->makeup = (float*)data;
			break;
		case ACOMP_GAINR:
			acomp->gainr = (float*)data;
			break;
		case ACOMP_OUTLEVEL:
			acomp->outlevel = (float*)data;
			break;
		case ACOMP_INLEVEL:
			acomp->inlevel = (float*)data;
			break;
		case ACOMP_SIDECHAIN:
			acomp->sidechain = (float*)data;
			break;
		case ACOMP_ENABLE:
			acomp->enable = (float*)data;
			break;
		default:
			break;
	}
}

static void
connect_mono(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	AComp* acomp = (AComp*)instance;
	connect_port (instance, port, data);

	switch ((PortIndex)port) {
		case ACOMP_A0:
			acomp->input0 = (float*)data;
			break;
		case ACOMP_A1:
			acomp->sc = (float*)data;
			break;
		case ACOMP_A2:
			acomp->output0 = (float*)data;
			break;
	default:
		break;
	}
}

static void
connect_stereo(LV2_Handle instance,
               uint32_t port,
               void* data)
{
	AComp* acomp = (AComp*)instance;
	connect_port (instance, port, data);

	switch ((PortIndex)port) {
		case ACOMP_A0:
			acomp->input0 = (float*)data;
			break;
		case ACOMP_A1:
			acomp->input1 = (float*)data;
			break;
		case ACOMP_A2:
			acomp->sc = (float*)data;
			break;
		case ACOMP_A3:
			acomp->output0 = (float*)data;
			break;
		case ACOMP_A4:
			acomp->output1 = (float*)data;
			break;
	default:
		break;
	}
}

// Force already-denormal float value to zero
static inline float
sanitize_denormal(float value) {
	if (!isnormal(value)) {
		value = 0.f;
	}
	return value;
}

static inline float
from_dB(float gdb) {
	return powf (10.0f, 0.05f * gdb);
}

static inline float
to_dB(float g) {
	return (20.f * log10f (g));
}

static void
activate(LV2_Handle instance)
{
	AComp* acomp = (AComp*)instance;

	*(acomp->gainr) = 0.0f;
	*(acomp->outlevel) = -70.0f;
	*(acomp->inlevel) = -160.f;

#ifdef LV2_EXTENDED
	acomp->v_peakdb = -160.f;
	acomp->peakdb_samples = 0;
#endif
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	AComp* acomp = (AComp*)instance;

	const float* const ins[2] = { acomp->input0, acomp->input1 };
	const float* const sc = acomp->sc;
	float* const outs[2] = { acomp->output0, acomp->output1 };

	float srate = acomp->srate;
	float width = (6.f * *(acomp->knee)) + 0.01;
	float attack_coeff = expf (-1000.f / (*(acomp->attack) * srate));
	float release_coeff = expf (-1000.f / (*(acomp->release) * srate));

	float max_out = 0.f;
	float Lgain = 1.f;
	float Lxg, Lyg;
	float current_gainr;
	float old_gainr = *acomp->gainr;

	int usesidechain = (*(acomp->sidechain) <= 0.f) ? 0 : 1;
	uint32_t i;
	float ingain;
	float sc0;
	float maxabs;

	uint32_t n_channels = acomp->n_channels;

	float ratio = *acomp->ratio;
	float thresdb = *acomp->thresdb;
	float makeup = *acomp->makeup;
	float makeup_target = from_dB(makeup);
	float makeup_gain = acomp->makeup_gain;

	const float tau = (1.f - expf (-2.f * M_PI * 25.f / acomp->srate));

	if (*acomp->enable <= 0) {
		ratio = 1.f;
		thresdb = 0.f;
		makeup = 0.f;
		makeup_target = 1.f;
	}

#ifdef LV2_EXTENDED
	if (acomp->v_knee != *acomp->knee) {
		acomp->v_knee = *acomp->knee;
		acomp->need_expose = true;
	}

	if (acomp->v_ratio != ratio) {
		acomp->v_ratio = ratio;
		acomp->need_expose = true;
	}

	if (acomp->v_thresdb != thresdb) {
		acomp->v_thresdb = thresdb;
		acomp->need_expose = true;
	}

	if (acomp->v_makeup != makeup) {
		acomp->v_makeup = makeup;
		acomp->need_expose = true;
	}
#endif

	float in_peak_db = -160.f;
	float max_gainr = 0.f;

	for (i = 0; i < n_samples; i++) {
		maxabs = 0.f;
		for (uint32_t c=0; c<n_channels; ++c) {
			maxabs = fmaxf(fabsf(ins[c][i]), maxabs);
		}
		sc0 = sc[i];
		ingain = usesidechain ? fabs(sc0) : maxabs;
		Lyg = 0.f;
		Lxg = (ingain==0.f) ? -160.f : to_dB(ingain);
		Lxg = sanitize_denormal(Lxg);

		if (Lxg > in_peak_db) {
			in_peak_db = Lxg;
		}

		if (2.f*(Lxg-thresdb) < -width) {
			Lyg = Lxg;
		} else if (2.f*(Lxg-thresdb) > width) {
			Lyg = thresdb + (Lxg-thresdb)/ratio;
			Lyg = sanitize_denormal(Lyg);
		} else {
			Lyg = Lxg + (1.f/ratio-1.f)*(Lxg-thresdb+width/2.f)*(Lxg-thresdb+width/2.f)/(2.f*width);
		}

		current_gainr = Lxg - Lyg;

		if (current_gainr < old_gainr) {
			current_gainr = release_coeff*old_gainr + (1.f-release_coeff)*current_gainr;
		} else if (current_gainr > old_gainr) {
			current_gainr = attack_coeff*old_gainr + (1.f-attack_coeff)*current_gainr;
		}

		current_gainr = sanitize_denormal(current_gainr);

		Lgain = from_dB(-current_gainr);

		old_gainr = current_gainr;

		*(acomp->gainr) = current_gainr;
		if (current_gainr > max_gainr) {
			max_gainr = current_gainr;
		}

		makeup_gain += tau * (makeup_target - makeup_gain);

		for (uint32_t c=0; c<n_channels; ++c) {
			float out = ins[c][i] * Lgain * makeup_gain;
			outs[c][i] = out;
			out = fabsf (out);
			if (out > max_out) {
				max_out = out;
				sanitize_denormal(max_out);
			}
		}
	}

	if (fabsf(tau * (makeup_gain - makeup_target)) < FLT_EPSILON*makeup_gain) {
		makeup_gain = makeup_target;
	}

	*(acomp->outlevel) = (max_out < MINUS_60) ? -60.f : to_dB(max_out);
	*(acomp->inlevel) = in_peak_db;
	acomp->makeup_gain = makeup_gain;

#ifdef LV2_EXTENDED
	acomp->v_gainr = max_gainr;

	if (in_peak_db > acomp->v_peakdb) {
		acomp->v_peakdb = in_peak_db;
		acomp->peakdb_samples = 0;
	} else {
		acomp->peakdb_samples += n_samples;
		if ((float)acomp->peakdb_samples/acomp->srate > RESET_PEAK_AFTER_SECONDS) {
			acomp->v_peakdb = in_peak_db;
			acomp->peakdb_samples = 0;
			acomp->need_expose = true;
		}
	}

	const float v_lvl_in = in_peak_db;
	const float v_lvl_out = *acomp->outlevel;

	float state_x;

	const float knee_lim_gr = (1.f - 1.f/ratio) * width/2.f;

	if (acomp->v_gainr > knee_lim_gr) {
		state_x = acomp->v_gainr / (1.f - 1.f/ratio) + thresdb;
	} else {
		state_x = sqrtf ( (2.f*width*acomp->v_gainr) / (1.f-1.f/ratio) ) + thresdb - width/2.f;
	}

	if (fabsf (acomp->v_lvl_out - v_lvl_out) >= .1f ||
	    fabsf (acomp->v_lvl_in - v_lvl_in) >= .1f ||
	    fabsf (acomp->v_state_x - state_x) >= .1f ) {
		// >= 0.1dB difference
		acomp->need_expose = true;
		acomp->v_lvl_in = v_lvl_in;
		acomp->v_lvl_out = v_lvl_out;
		acomp->v_state_x = state_x;
	}
	if (acomp->need_expose && acomp->queue_draw) {
		acomp->need_expose = false;
		acomp->queue_draw->queue_draw (acomp->queue_draw->handle);
	}
#endif
}

static void
deactivate(LV2_Handle instance)
{
	activate(instance);
}

static void
cleanup(LV2_Handle instance)
{
#ifdef LV2_EXTENDED
	AComp* acomp = (AComp*)instance;
	if (acomp->display) {
		cairo_surface_destroy (acomp->display);
	}
#endif

	free(instance);
}


#ifndef MIN
#define MIN(A,B) ((A) < (B)) ? (A) : (B)
#endif

#ifdef LV2_EXTENDED
static float
comp_curve (const AComp* self, float xg) {
	const float knee = self->v_knee;
	const float ratio = self->v_ratio;
	const float thresdb = self->v_thresdb;
	const float makeup = self->v_makeup;

	const float width = 6.f * knee + 0.01f;
	float yg = 0.f;

	if (2.f * (xg - thresdb) < -width) {
		yg = xg;
	} else if (2.f * (xg - thresdb) > width) {
		yg = thresdb + (xg - thresdb) / ratio;
	} else {
		yg = xg + (1.f / ratio - 1.f ) * (xg - thresdb + width / 2.f) * (xg - thresdb + width / 2.f) / (2.f * width);
	}

	yg += makeup;

	return yg;
}


#include "dynamic_display.c"

static void
render_inline_full (cairo_t* cr, const AComp* self)
{
	const float w = self->w;
	const float h = self->h;

	const float makeup_thres = self->v_thresdb + self->v_makeup;

	draw_grid (cr, w,h);

	if (self->v_thresdb < 0) {
		const float x = w * (1.f - (10.f-self->v_thresdb)/70.f) + 0.5;
		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);
	}

	draw_GR_bar (cr, w,h, self->v_gainr);

	// draw state
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);

	const float state_x = w * (1.f - (10.f-(*self->inlevel))/70.f);
	const float state_y = h * ((*self->outlevel) - 10.f) / -70.f;

	cairo_arc (cr, state_x, state_y, 6.f, 0.f, 2.f*M_PI);
	cairo_fill (cr);

	// draw curve
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
	cairo_move_to (cr, 0, h);

	for (uint32_t x = 0; x < w; ++x) {
		// plot -60..+10  dB
		const float x_db = 70.f * (-1.f + x / (float)w) + 10.f;
		const float y_db = comp_curve (self, x_db) - 10.f;
		const float y = h * (y_db / -70.f);
		cairo_line_to (cr, x, y);
	}
	cairo_stroke_preserve (cr);

	cairo_line_to (cr, w, h);
	cairo_close_path (cr);
	cairo_clip (cr);

	// draw signal level & reduction/gradient
	const float top = comp_curve (self, 0) - 10.f;
	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, h);
	if (top > makeup_thres - 10.f) {
		cairo_pattern_add_color_stop_rgba (pat, 0.0, 0.8, 0.1, 0.1, 0.5);
		cairo_pattern_add_color_stop_rgba (pat, top / -70.f, 0.8, 0.1, 0.1, 0.5);
	}
	if (self->v_knee > 0) {
		cairo_pattern_add_color_stop_rgba (pat, ((makeup_thres -10.f) / -70.f), 0.7, 0.7, 0.2, 0.5);
		cairo_pattern_add_color_stop_rgba (pat, ((makeup_thres - self->v_knee - 10.f) / -70.f), 0.5, 0.5, 0.5, 0.5);
	} else {
		cairo_pattern_add_color_stop_rgba (pat, ((makeup_thres - 10.f)/ -70.f), 0.7, 0.7, 0.2, 0.5);
		cairo_pattern_add_color_stop_rgba (pat, ((makeup_thres - 10.01f) / -70.f), 0.5, 0.5, 0.5, 0.5);
	}
	cairo_pattern_add_color_stop_rgba (pat, 1.0, 0.5, 0.5, 0.5, 0.5);

	// maybe cut off at x-position?
	const float x = w * (self->v_lvl_in + 60) / 70.f;
	const float y = x + h*self->v_makeup;
	cairo_rectangle (cr, 0, h - y, x, y);
	if (self->v_ratio > 1.0) {
		cairo_set_source (cr, pat);
	} else {
		cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
	}
	cairo_fill (cr);

	cairo_pattern_destroy (pat); // TODO cache pattern
}

static void
render_inline_only_bars (cairo_t* cr, const AComp* self)
{
	draw_inline_bars (cr, self->w, self->h,
			  self->v_thresdb, self->v_ratio,
			  self->v_peakdb, self->v_gainr,
			  self->v_lvl_in, self->v_lvl_out);
}

static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	AComp* self = (AComp*)instance;

	uint32_t h = MIN (w, max_h);
	if (w < 200) {
		h = 40;
	}

	if (!self->display || self->w != w || self->h != h) {
		if (self->display) cairo_surface_destroy(self->display);
		self->display = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		self->w = w;
		self->h = h;
	}

	cairo_t* cr = cairo_create (self->display);

	if (w >= 200) {
		render_inline_full (cr, self);
	} else {
		render_inline_only_bars (cr, self);
	}

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

static const LV2_Descriptor descriptor_mono = {
	ACOMP_URI,
	instantiate,
	connect_mono,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor_stereo = {
	ACOMP_STEREO_URI,
	instantiate,
	connect_stereo,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor_mono;
	case 1:
		return &descriptor_stereo;
	default:
		return NULL;
	}
}
