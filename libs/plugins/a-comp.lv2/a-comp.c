/* a-comp
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ACOMP_URI "urn:ardour:a-comp"

typedef enum {
	ACOMP_INPUT0 = 0,
	ACOMP_INPUT1,
	ACOMP_OUTPUT,

	ACOMP_ATTACK,
	ACOMP_RELEASE,
	ACOMP_KNEE,
	ACOMP_RATIO,
	ACOMP_THRESHOLD,
	ACOMP_MAKEUP,

	ACOMP_GAINR,
	ACOMP_OUTLEVEL,
	ACOMP_SIDECHAIN,
} PortIndex;


typedef struct {
	float* input0;
	float* input1;
	float* output;

	float* attack;
	float* release;
	float* knee;
	float* ratio;
	float* thresdb;
	float* makeup;

	float* gainr;
	float* outlevel;
	float* sidechain;

	float srate;
	float old_yl;
	float old_y1;
	float old_yg;

#ifdef LV2_EXTENDED
	bool                     need_expose;
	LV2_Inline_Display_Image_Surface surf;
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
	float v_lvl;
#endif
} AComp;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	AComp* acomp = (AComp*)calloc(1, sizeof(AComp));

	for (int i=0; features[i]; ++i) {
#ifdef LV2_EXTENDED
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			acomp->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
#endif
	}

	acomp->srate = rate;
	acomp->old_yl=acomp->old_y1=acomp->old_yg=0.f;
	acomp->need_expose = true;

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
	case ACOMP_SIDECHAIN:
		acomp->sidechain = (float*)data;
		break;
	case ACOMP_INPUT0:
		acomp->input0 = (float*)data;
		break;
	case ACOMP_INPUT1:
		acomp->input1 = (float*)data;
		break;
	case ACOMP_OUTPUT:
		acomp->output = (float*)data;
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
	AComp* acomp = (AComp*)instance;

	*(acomp->gainr) = 0.0f;
	*(acomp->outlevel) = -45.0f;
	acomp->old_yl=acomp->old_y1=acomp->old_yg=0.f;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	AComp* acomp = (AComp*)instance;

	const float* const input0 = acomp->input0;
	const float* const input1 = acomp->input1;
	float* const output = acomp->output;

	float srate = acomp->srate;
	float width = (6.f * *(acomp->knee)) + 0.01;
	float cdb=0.f;
	float attack_coeff = exp(-1000.f/(*(acomp->attack) * srate));
	float release_coeff = exp(-1000.f/(*(acomp->release) * srate));

	float max = 0.f;
	float lgaininp = 0.f;
	float Lgain = 1.f;
	float Lxg, Lxl, Lyg, Lyl, Ly1;
	int usesidechain = (*(acomp->sidechain) < 0.5) ? 0 : 1;
	uint32_t i;
	float ingain;
	float in0;
	float in1;
	float ratio = *(acomp->ratio);
	float thresdb = *(acomp->thresdb);

#ifdef LV2_EXTENDED
	if (acomp->v_knee != *acomp->knee) {
		acomp->v_knee = *acomp->knee;
		acomp->need_expose = true;
	}

	if (acomp->v_ratio != *acomp->ratio) {
		acomp->v_ratio = *acomp->ratio;
		acomp->need_expose = true;
	}

	if (acomp->v_thresdb != *acomp->thresdb) {
		acomp->v_thresdb = *acomp->thresdb;
		acomp->need_expose = true;
	}
#endif

	for (i = 0; i < n_samples; i++) {
		in0 = input0[i];
		in1 = input1[i];
		ingain = usesidechain ? in1 : in0;
		Lyg = 0.f;
		Lxg = (ingain==0.f) ? -160.f : to_dB(fabs(ingain));
		Lxg = sanitize_denormal(Lxg);

		Lyg = Lxg + (1.f/ratio-1.f)*(Lxg-thresdb+width/2.f)*(Lxg-thresdb+width/2.f)/(2.f*width);

		if (2.f*(Lxg-thresdb) < -width) {
			Lyg = Lxg;
		} else {
			Lyg = thresdb + (Lxg-thresdb)/ratio;
			Lyg = sanitize_denormal(Lyg);
		}

		Lxl = Lxg - Lyg;

		acomp->old_y1 = sanitize_denormal(acomp->old_y1);
		acomp->old_yl = sanitize_denormal(acomp->old_yl);
		Ly1 = fmaxf(Lxl, release_coeff * acomp->old_y1+(1.f-release_coeff)*Lxl);
		Lyl = attack_coeff * acomp->old_yl+(1.f-attack_coeff)*Ly1;
		Ly1 = sanitize_denormal(Ly1);
		Lyl = sanitize_denormal(Lyl);

		cdb = -Lyl;
		Lgain = from_dB(cdb);

		*(acomp->gainr) = Lyl;

		lgaininp = in0 * Lgain;
		output[i] = lgaininp * from_dB(*(acomp->makeup));

		max = (fabsf(output[i]) > max) ? fabsf(output[i]) : sanitize_denormal(max);

		// TODO re-use local variables on stack
		// store values back to acomp at the end of the inner-loop
		acomp->old_yl = Lyl;
		acomp->old_y1 = Ly1;
		acomp->old_yg = Lyg;
	}

	*(acomp->outlevel) = (max == 0.f) ? -45.f : to_dB(max);

#ifdef LV2_EXTENDED
	// XXX Lyg is not correct, we need input-level filtered by attack/release
	if (fabsf (acomp->v_lvl - Lyg) >= 1) { // quantize to 1dB difference
		acomp->need_expose = true;
		acomp->v_lvl = Lyg;
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
comp_curve (AComp* self, float xg) {
	const float knee = self->v_knee;
	const float ratio = self->v_ratio;
	const float thresdb = self->v_thresdb;

	const float width = 6.f * knee + 0.01f;
	float yg = 0.f;

	if (2.f * (xg - thresdb) < -width) {
		yg = xg;
	} else if (2.f * fabs (xg - thresdb) <= width) {
		yg = xg + (1.f / ratio - 1.f ) * (xg - thresdb + width / 2.f) * (xg - thresdb + width / 2.f) / (2.f * width);
	} else if (2.f * (xg - thresdb) > width) {
		yg = thresdb + (xg - thresdb) / ratio;
	}
	return yg;
}


static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	AComp* self = (AComp*)instance;
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

	// draw grid 10dB steps
	const double dash2[] = {1, 3};
	cairo_save (cr);
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	for (uint32_t d = 1; d < 6; ++d) {
		const float x = -.5 + floorf (w * (d * 10.f / 60.f));
		const float y = -.5 + floorf (h * (d * 10.f / 60.f));

		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);

		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	cairo_restore (cr);


	// draw curve
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
	cairo_move_to (cr, 0, h);

	for (uint32_t x = 0; x < w; ++x) {
		// plot -60..0  dB
		const float x_db = 60.f * (-1.f + x / (float)w);
		const float y_db = comp_curve (self, x_db);
		const float y = h * (y_db / -60.f);
		cairo_line_to (cr, x, y);
	}
	cairo_stroke_preserve (cr);

	cairo_line_to (cr, w, h);
	cairo_close_path (cr);
	cairo_clip (cr);

	// draw signal level
	const float x = w * (self->v_lvl + 60) / 60.f;
	cairo_rectangle (cr, 0, 0, x, h);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
	cairo_fill (cr);


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
	static const LV2_Inline_Display_Interface display  = { render_inline };
	if (!strcmp(uri, LV2_INLINEDISPLAY__interface)) {
		return &display;
	}
	return NULL;
}

static const LV2_Descriptor descriptor = {
	ACOMP_URI,
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
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
