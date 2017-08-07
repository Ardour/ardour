/* a-exp
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
 * based on a-comp (C) 2016 Damien Zammit <damien@zamaudio.com>
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



#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define RESET_PEAK_AFTER_SECONDS 3

#define AEXP_URI "urn:ardour:a-exp"
#define AEXP_STEREO_URI "urn:ardour:a-exp#stereo"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#ifdef COMPILER_MSVC
#include <float.h>
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

typedef enum {
	AEXP_ATTACK = 0,
	AEXP_RELEASE,
	AEXP_KNEE,
	AEXP_RATIO,
	AEXP_THRESHOLD,
	AEXP_MAKEUP,

	AEXP_GAINR,
	AEXP_OUTLEVEL,
	AEXP_INLEVEL,
	AEXP_SIDECHAIN,
	AEXP_ENABLE,

	AEXP_A0,
	AEXP_A1,
	AEXP_A2,
	AEXP_A3,
	AEXP_A4,
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

	float srate;

	float makeup_gain;
	float tau;

	bool was_disabled;

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

	float v_peakdb;
	uint32_t peakdb_samples;
#endif
} AExp;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	AExp* aexp = (AExp*)calloc(1, sizeof(AExp));

	for (int i=0; features[i]; ++i) {
#ifdef LV2_EXTENDED
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			aexp->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
#endif
	}

	aexp->srate = rate;
	aexp->tau = (1.0 - exp (-2.f * M_PI * 25.f / aexp->srate));
#ifdef LV2_EXTENDED
	aexp->need_expose = true;
	aexp->v_lvl_out = -70.f;
#endif

	return (LV2_Handle)aexp;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	AExp* aexp = (AExp*)instance;

	switch ((PortIndex)port) {
		case AEXP_ATTACK:
			aexp->attack = (float*)data;
			break;
		case AEXP_RELEASE:
			aexp->release = (float*)data;
			break;
		case AEXP_KNEE:
			aexp->knee = (float*)data;
			break;
		case AEXP_RATIO:
			aexp->ratio = (float*)data;
			break;
		case AEXP_THRESHOLD:
			aexp->thresdb = (float*)data;
			break;
		case AEXP_MAKEUP:
			aexp->makeup = (float*)data;
			break;
		case AEXP_GAINR:
			aexp->gainr = (float*)data;
			break;
		case AEXP_OUTLEVEL:
			aexp->outlevel = (float*)data;
			break;
		case AEXP_INLEVEL:
			aexp->inlevel = (float*)data;
			break;
		case AEXP_SIDECHAIN:
			aexp->sidechain = (float*)data;
			break;
		case AEXP_ENABLE:
			aexp->enable = (float*)data;
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
	AExp* aexp = (AExp*)instance;
	connect_port (instance, port, data);

	switch ((PortIndex)port) {
		case AEXP_A0:
			aexp->input0 = (float*)data;
			break;
		case AEXP_A1:
			aexp->sc = (float*)data;
			break;
		case AEXP_A2:
			aexp->output0 = (float*)data;
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
	AExp* aexp = (AExp*)instance;
	connect_port (instance, port, data);

	switch ((PortIndex)port) {
		case AEXP_A0:
			aexp->input0 = (float*)data;
			break;
		case AEXP_A1:
			aexp->input1 = (float*)data;
			break;
		case AEXP_A2:
			aexp->sc = (float*)data;
			break;
		case AEXP_A3:
			aexp->output0 = (float*)data;
			break;
		case AEXP_A4:
			aexp->output1 = (float*)data;
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
	return (exp(gdb/20.f*log(10.f)));
}

static inline float
to_dB(float g) {
	return (20.f*log10(g));
}

static void
activate(LV2_Handle instance)
{
	AExp* aexp = (AExp*)instance;

	*(aexp->gainr) = 160.0f;
	*(aexp->outlevel) = -45.0f;
	*(aexp->inlevel) = -45.0f;

#ifdef LV2_EXTENDED
	aexp->v_peakdb = -160.f;
	aexp->peakdb_samples = 0;
#endif
}

static void
run_mono(LV2_Handle instance, uint32_t n_samples)
{
	AExp* aexp = (AExp*)instance;

	const float* const input = aexp->input0;
	const float* const sc = aexp->sc;
	float* const output = aexp->output0;

	float srate = aexp->srate;
	float width = (6.f * *(aexp->knee)) + 0.01;
	float attack_coeff = exp(-1000.f/(*(aexp->attack) * srate));
	float release_coeff = exp(-1000.f/(*(aexp->release) * srate));

	float max = 0.f;
	float lgaininp = 0.f;
	float Lgain = 1.f;
	float Lxg, Lyg;
	float current_gainr;
	float old_gainr;

	int usesidechain = (*(aexp->sidechain) <= 0.f) ? 0 : 1;
	uint32_t i;
	float ingain;
	float in0;
	float sc0;

	float ratio = *aexp->ratio;
	float thresdb = *aexp->thresdb;
	float makeup = *aexp->makeup;
	float makeup_target = from_dB(makeup);
	float makeup_gain = aexp->makeup_gain;

	const float tau = aexp->tau;

	if (*aexp->enable <= 0) {
		ratio = 1.f;
		thresdb = 0.f;
		makeup = 0.f;
		makeup_target = 1.f;
		if (!aexp->was_disabled) {
			*aexp->gainr = 0.f;
			aexp->was_disabled = true;
		}
	} else {
		if (aexp->was_disabled) {
			*aexp->gainr = 160.f;
			aexp->was_disabled = false;
		}
	}

#ifdef LV2_EXTENDED
	if (aexp->v_knee != *aexp->knee) {
		aexp->v_knee = *aexp->knee;
		aexp->need_expose = true;
	}

	if (aexp->v_ratio != ratio) {
		aexp->v_ratio = ratio;
		aexp->need_expose = true;
	}

	if (aexp->v_thresdb != thresdb) {
		aexp->v_thresdb = thresdb;
		aexp->need_expose = true;
	}

	if (aexp->v_makeup != makeup) {
		aexp->v_makeup = makeup;
		aexp->need_expose = true;
	}
#endif
	float in_peak_db = -160.f;
	old_gainr = *aexp->gainr;
	float max_gainr = 0.0;

	for (i = 0; i < n_samples; i++) {
		in0 = input[i];
		sc0 = sc[i];
		ingain = usesidechain ? fabs(sc0) : fabs(in0);
		Lyg = 0.f;
		Lxg = (ingain==0.f) ? -160.f : to_dB(ingain);
		Lxg = sanitize_denormal(Lxg);

		if (Lxg > in_peak_db) {
			in_peak_db = Lxg;
		}

		if (2.f*(Lxg-thresdb) < -width) {
			Lyg = thresdb + (Lxg-thresdb) * ratio;
			Lyg = sanitize_denormal(Lyg);
		} else if (2.f*(Lxg-thresdb) > width) {
			Lyg = Lxg;
		} else {
			Lyg = Lxg + (1.f-ratio)*(Lxg-thresdb-width/2.f)*(Lxg-thresdb-width/2.f)/(2.f*width);
		}

		current_gainr = Lxg - Lyg;

		if (current_gainr > 160.f) {
			current_gainr = 160.f;
		}

		if (current_gainr > old_gainr) {
			current_gainr = release_coeff*old_gainr + (1.f-release_coeff)*current_gainr;
		} else if (current_gainr < old_gainr) {
			current_gainr = attack_coeff*old_gainr + (1.f-attack_coeff)*current_gainr;
		}

		current_gainr = sanitize_denormal(current_gainr);

		Lgain = from_dB(-current_gainr);

		old_gainr = current_gainr;

		*(aexp->gainr) = current_gainr;
		if (current_gainr > max_gainr) {
			max_gainr = current_gainr;
		}

		lgaininp = in0 * Lgain;

		makeup_gain += tau * (makeup_target - makeup_gain) + 1e-12;
		output[i] = lgaininp * makeup_gain;

		max = (fabsf(output[i]) > max) ? fabsf(output[i]) : sanitize_denormal(max);
	}

	*(aexp->outlevel) = (max < 0.0056f) ? -45.f : to_dB(max);
	*(aexp->inlevel) = in_peak_db;
	aexp->makeup_gain = makeup_gain;

#ifdef LV2_EXTENDED
	if (in_peak_db > aexp->v_peakdb) {
		aexp->v_peakdb = in_peak_db;
		aexp->peakdb_samples = 0;
	} else {
		aexp->peakdb_samples += n_samples;
		if ((float)aexp->peakdb_samples/aexp->srate > RESET_PEAK_AFTER_SECONDS) {
			aexp->v_peakdb = in_peak_db;
			aexp->peakdb_samples = 0;
			aexp->need_expose = true;
		}
	}

	const float v_lvl_out = (max < 0.001f) ? -1600.f : to_dB(max);
	const float v_lvl_in = in_peak_db;

	if (fabsf (aexp->v_lvl_out - v_lvl_out) >= .1 ||
	    fabsf (aexp->v_lvl_in - v_lvl_in) >= .1 ||
	    fabsf (aexp->v_gainr - max_gainr) >= .1) {
		// >= 0.1dB difference
		aexp->need_expose = true;
		aexp->v_lvl_in = v_lvl_in;
		aexp->v_lvl_out = v_lvl_out;
		aexp->v_gainr = max_gainr;
	}
	if (aexp->need_expose && aexp->queue_draw) {
		aexp->need_expose = false;
		aexp->queue_draw->queue_draw (aexp->queue_draw->handle);
	}
#endif
}


static void
run_stereo(LV2_Handle instance, uint32_t n_samples)
{
	AExp* aexp = (AExp*)instance;

	const float* const input0 = aexp->input0;
	const float* const input1 = aexp->input1;
	const float* const sc = aexp->sc;
	float* const output0 = aexp->output0;
	float* const output1 = aexp->output1;

	float srate = aexp->srate;
	float width = (6.f * *(aexp->knee)) + 0.01;
	float attack_coeff = exp(-1000.f/(*(aexp->attack) * srate));
	float release_coeff = exp(-1000.f/(*(aexp->release) * srate));

	float max = 0.f;
	float lgaininp = 0.f;
	float rgaininp = 0.f;
	float Lgain = 1.f;
	float Lxg, Lyg;
	float current_gainr;
	float old_gainr = *aexp->gainr;

	int usesidechain = (*(aexp->sidechain) <= 0.f) ? 0 : 1;
	uint32_t i;
	float ingain;
	float in0;
	float in1;
	float sc0;
	float maxabslr;

	float ratio = *aexp->ratio;
	float thresdb = *aexp->thresdb;
	float makeup = *aexp->makeup;
	float makeup_target = from_dB(makeup);
	float makeup_gain = aexp->makeup_gain;

	const float tau = aexp->tau;

	if (*aexp->enable <= 0) {
		ratio = 1.f;
		thresdb = 0.f;
		makeup = 0.f;
		makeup_target = 1.f;
		if (!aexp->was_disabled) {
			*aexp->gainr = 0.f;
			aexp->was_disabled = true;
		}
	} else {
		if (aexp->was_disabled) {
			*aexp->gainr = 160.f;
			aexp->was_disabled = false;
		}
	}

#ifdef LV2_EXTENDED
	if (aexp->v_knee != *aexp->knee) {
		aexp->v_knee = *aexp->knee;
		aexp->need_expose = true;
	}

	if (aexp->v_ratio != ratio) {
		aexp->v_ratio = ratio;
		aexp->need_expose = true;
	}

	if (aexp->v_thresdb != thresdb) {
		aexp->v_thresdb = thresdb;
		aexp->need_expose = true;
	}

	if (aexp->v_makeup != makeup) {
		aexp->v_makeup = makeup;
		aexp->need_expose = true;
	}
#endif

	float in_peak_db = -160.f;
	old_gainr = *aexp->gainr;
	float max_gainr = 0.0;

	for (i = 0; i < n_samples; i++) {
		in0 = input0[i];
		in1 = input1[i];
		sc0 = sc[i];
		maxabslr = fmaxf(fabs(in0), fabs(in1));
		ingain = usesidechain ? fabs(sc0) : maxabslr;
		Lyg = 0.f;
		Lxg = (ingain==0.f) ? -160.f : to_dB(ingain);
		Lxg = sanitize_denormal(Lxg);

		if (Lxg > in_peak_db) {
			in_peak_db = Lxg;
		}

		if (2.f*(Lxg-thresdb) < -width) {
			Lyg = thresdb + (Lxg-thresdb) * ratio;
			Lyg = sanitize_denormal(Lyg);
		} else if (2.f*(Lxg-thresdb) > width) {
			Lyg = Lxg;
		} else {
			Lyg = Lxg + (1.f-ratio)*(Lxg-thresdb-width/2.f)*(Lxg-thresdb-width/2.f)/(2.f*width);
		}

		current_gainr = Lxg - Lyg;

		if (current_gainr > 160.f) {
			current_gainr = 160.f;
		}

		if (current_gainr > old_gainr) {
			current_gainr = release_coeff*old_gainr + (1.f-release_coeff)*current_gainr;
		} else if (current_gainr < old_gainr) {
			current_gainr = attack_coeff*old_gainr + (1.f-attack_coeff)*current_gainr;
		}

		current_gainr = sanitize_denormal(current_gainr);

		Lgain = from_dB(-current_gainr);

		old_gainr = current_gainr;

		*(aexp->gainr) = current_gainr;
		if (current_gainr > max_gainr) {
			max_gainr = current_gainr;
		}

		lgaininp = in0 * Lgain;
		rgaininp = in1 * Lgain;

		makeup_gain += tau * (makeup_target - makeup_gain) + 1e-12;

		output0[i] = lgaininp * makeup_gain;
		output1[i] = rgaininp * makeup_gain;

		max = (fmaxf(fabs(output0[i]), fabs(output1[i])) > max) ? fmaxf(fabs(output0[i]), fabs(output1[i])) : sanitize_denormal(max);
	}

	*(aexp->outlevel) = (max < 0.0056f) ? -45.f : to_dB(max);
	*(aexp->inlevel) = in_peak_db;
	aexp->makeup_gain = makeup_gain;

#ifdef LV2_EXTENDED
	if (in_peak_db > aexp->v_peakdb) {
		aexp->v_peakdb = in_peak_db;
		aexp->peakdb_samples = 0;
	} else {
		aexp->peakdb_samples += n_samples;
		if ((float)aexp->peakdb_samples/aexp->srate > RESET_PEAK_AFTER_SECONDS) {
			aexp->v_peakdb = in_peak_db;
			aexp->peakdb_samples = 0;
			aexp->need_expose = true;
		}
	}

	const float v_lvl_out = (max < 0.001f) ? -1600.f : to_dB(max);
	const float v_lvl_in = in_peak_db;

	if (fabsf (aexp->v_lvl_out - v_lvl_out) >= .1 ||
	    fabsf (aexp->v_lvl_in - v_lvl_in) >= .1 ||
	    fabsf (aexp->v_gainr - max_gainr) >= .1) {
		// >= 0.1dB difference
		aexp->need_expose = true;
		aexp->v_lvl_in = v_lvl_in;
		aexp->v_lvl_out = v_lvl_out;
		aexp->v_gainr = max_gainr;
	}
	if (aexp->need_expose && aexp->queue_draw) {
		aexp->need_expose = false;
		aexp->queue_draw->queue_draw (aexp->queue_draw->handle);
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
	AExp* aexp = (AExp*)instance;
	if (aexp->display) {
		cairo_surface_destroy (aexp->display);
	}
#endif

	free(instance);
}


#ifndef MIN
#define MIN(A,B) ((A) < (B)) ? (A) : (B)
#endif

#ifdef LV2_EXTENDED
static float
exp_curve (const AExp* self, float xg) {
	const float knee = self->v_knee;
	const float ratio = self->v_ratio;
	const float thresdb = self->v_thresdb;
	const float makeup = self->v_makeup;

	const float width = 6.f * knee + 0.01f;
	float yg = 0.f;

	if (2.f * (xg - thresdb) < -width) {
		yg = thresdb + (xg - thresdb) * ratio;
	} else if (2.f * (xg - thresdb) > width) {
		yg = xg;
	} else {
		yg = xg + (1.f - ratio) * (xg - thresdb - width / 2.f) * (xg - thresdb - width / 2.f) / (2.f * width);
	}

	yg += makeup;

	return yg;
}


static void
render_inline_full (cairo_t* cr, const AExp* self)
{
	const float w = self->w;
	const float h = self->h;

	const float makeup_thres = self->v_thresdb + self->v_makeup;

	// clear background
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);

	cairo_set_line_width(cr, 1.0);

	// draw grid 10dB steps
	const double dash1[] = {1, 2};
	const double dash2[] = {1, 3};
	cairo_save (cr);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	for (uint32_t d = 1; d < 7; ++d) {
		const float x = -.5 + floorf (w * (d * 10.f / 70.f));
		const float y = -.5 + floorf (h * (d * 10.f / 70.f));

		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);

		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 1.0);
	cairo_set_dash(cr, dash1, 2, 2);
	if (self->v_thresdb < 0) {
		const float y = -.5 + floorf (h * ((makeup_thres - 10.f) / -70.f));
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	// diagonal unity
	cairo_move_to (cr, 0, h);
	cairo_line_to (cr, w, 0);
	cairo_stroke (cr);
	cairo_restore (cr);

	{ // 0, 0
		cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
		const float x = -.5 + floorf (w * (60.f / 70.f));
		const float y = -.5 + floorf (h * (10.f / 70.f));
		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}

	{ // GR
		const float x = -.5 + floorf (w * (62.5f / 70.f));
		const float y = -.5 + floorf (h * (10.0f / 70.f));
		const float wd = floorf (w * (5.f / 70.f));
		const float ht = floorf (h * (55.f / 70.f));
		cairo_rectangle (cr, x, y, wd, ht);
		cairo_fill (cr);

		const float h_gr = fminf (ht, floorf (h * self->v_gainr / 70.f));
		cairo_set_source_rgba (cr, 0.95, 0.0, 0.0, 1.0);
		cairo_rectangle (cr, x, y, wd, h_gr);
		cairo_fill (cr);
		cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
		cairo_rectangle (cr, x, y, wd, ht);
		cairo_set_source_rgba (cr, 0.75, 0.75, 0.75, 1.0);
		cairo_stroke (cr);
	}

	// draw curve
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
	cairo_move_to (cr, 0, h);

	for (uint32_t x = 0; x < w; ++x) {
		// plot -60..+10  dB
		const float x_db = 70.f * (-1.f + x / (float)w) + 10.f;
		const float y_db = exp_curve (self, x_db) - 10.f;
		const float y = h * (y_db / -70.f);
		cairo_line_to (cr, x, y);
	}
	cairo_stroke_preserve (cr);

	cairo_line_to (cr, w, h);
	cairo_close_path (cr);
	cairo_clip (cr);

	// draw signal level & reduction/gradient
	const float top = exp_curve (self, 0) - 10.f;
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

	// draw peak input
	cairo_set_source_rgba (cr, .9f, .9f, .9f, 1.0);
	cairo_set_line_width(cr, 1.0);

	const float peak_x = w * (1.f - (10.f-self->v_peakdb)/70.f);
	const float peak_y = h * (exp_curve (self, self->v_peakdb) - 10.f) / -70.f;

	cairo_move_to(cr, peak_x, h);
	cairo_line_to(cr, peak_x, peak_y);
	cairo_stroke (cr);

	cairo_pattern_destroy (pat); // TODO cache pattern
}

static void
render_inline_only_bars (cairo_t* cr, const AExp* self)
{
	const float w = self->w;
	const float h = self->h;

	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);


	cairo_save (cr);

	const float ht = 0.25f * h;

	const float x1 = w*0.05;
	const float wd = w - 2.0f*x1;

	const float y1 = 0.17*h;
	const float y2 = h - y1 - ht;

	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	cairo_rectangle (cr, x1, y1, wd, ht);
	cairo_fill (cr);

	cairo_rectangle (cr, x1, y2, wd, ht);
	cairo_fill (cr);

	cairo_set_source_rgba (cr, 0.75, 0.0, 0.0, 1.0);
	const float w_gr = (self->v_gainr > 60.f) ? wd : wd * self->v_gainr * (1.f/60.f);
	cairo_rectangle (cr, x1+wd-w_gr, y2, w_gr, ht);
	cairo_fill (cr);

	if (self->v_lvl_in > -60.f) {
		if (self->v_lvl_out > 10.f) {
			cairo_set_source_rgba (cr, 0.75, 0.0, 0.0, 1.0);
		} else if (self->v_lvl_out > 0.f) {
			cairo_set_source_rgba (cr, 0.66, 0.66, 0.0, 1.0);
		} else {
			cairo_set_source_rgba (cr, 0.0, 0.66, 0.0, 1.0);
		}
		const float w_g = (self->v_lvl_in > 10.f) ? wd : wd * (60.f+self->v_lvl_in) / 70.f;
		cairo_rectangle (cr, x1, y1, w_g, ht);
		cairo_fill (cr);
	}

	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);

	const float tck = 0.33*ht;

	cairo_set_line_width (cr, .5);

	for (uint32_t d = 1; d < 7; ++d) {
		const float x = x1 + (d * wd * (10.f / 70.f));

		cairo_move_to (cr, x, y1);
		cairo_line_to (cr, x, y1+tck);

		cairo_move_to (cr, x, y1+ht);
		cairo_line_to (cr, x, y1+ht-tck);

		cairo_move_to (cr, x, y2);
		cairo_line_to (cr, x, y2+tck);

		cairo_move_to (cr, x, y2+ht);
		cairo_line_to (cr, x, y2+ht-tck);
	}

	cairo_stroke (cr);

	const float x_0dB = x1 + wd*(60.f/70.f);

	cairo_move_to (cr, x_0dB, y1);
	cairo_line_to (cr, x_0dB, y1+ht);

	cairo_rectangle (cr, x1, y1, wd, ht);
	cairo_rectangle (cr, x1, y2, wd, ht);
	cairo_stroke (cr);

	cairo_set_line_width (cr, 2.0);

	// visualize threshold
	const float tr = x1 + wd * (60.f+self->v_thresdb) / 70.f;
	cairo_set_source_rgba (cr, 0.95, 0.95, 0.0, 1.0);
	cairo_move_to (cr, tr, y1);
	cairo_line_to (cr, tr, y1+ht);
	cairo_stroke (cr);

	// visualize ratio
	const float reduced_0dB = self->v_thresdb * (1.f - 1.f/self->v_ratio);
	const float rt = x1 + wd * (60.f+reduced_0dB) / 70.f;
	cairo_set_source_rgba (cr, 0.95, 0.0, 0.0, 1.0);
	cairo_move_to (cr, rt, y1);
	cairo_line_to (cr, rt, y1+ht);
	cairo_stroke (cr);

	// visualize in peak
	if (self->v_peakdb > -60.f) {
		cairo_set_source_rgba (cr, 0.0, 1.0, 0.0, 1.0);
		const float pk = (self->v_peakdb > 10.f) ? x1+wd : wd * (60.f+self->v_peakdb) / 70.f;
		cairo_move_to (cr, pk, y1);
		cairo_line_to (cr, pk, y1+ht);
		cairo_stroke (cr);
	}
}


static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	AExp* self = (AExp*)instance;

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
	AEXP_URI,
	instantiate,
	connect_mono,
	activate,
	run_mono,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor_stereo = {
	AEXP_STEREO_URI,
	instantiate,
	connect_stereo,
	activate,
	run_stereo,
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
