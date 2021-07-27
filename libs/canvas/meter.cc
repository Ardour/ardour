/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#include <stdlib.h>

#include <glibmm.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/rgb_macros.h>

#include "canvas/canvas.h"
#include "gtkmm2ext/colors.h"
#include "canvas/meter.h"

using namespace Glib;
using namespace std;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;

int Meter::min_pattern_metric_size = 16;
int Meter::max_pattern_metric_size = 1024;
bool Meter::no_rgba_overlay = false;

Meter::Pattern10Map Meter::vm_pattern_cache;
Meter::PatternBgMap Meter::vb_pattern_cache;

Meter::Pattern10Map Meter::hm_pattern_cache;
Meter::PatternBgMap Meter::hb_pattern_cache;

Meter::Meter (Item* parent, long hold, unsigned long dimen, Orientation o, int len,
		int clr0, int clr1, int clr2, int clr3,
		int clr4, int clr5, int clr6, int clr7,
		int clr8, int clr9,
		int bgc0, int bgc1,
		int bgh0, int bgh1,
		float stp0, float stp1,
		float stp2, float stp3,
		int styleflags
		)
	: Item (parent)
	, pixheight(0)
	, pixwidth(0)
	, _styleflags(styleflags)
	, orientation(o)
	, hold_cnt(hold)
	, hold_state(0)
	, bright_hold(false)
	, current_level(0)
	, current_peak(0)
	, highlight(false)
{
	init (clr0, clr1, clr2, clr3, clr4, clr5, clr6, clr7, clr8, clr9, bgc0, bgc1, bgh0, bgh1, stp0, stp1, stp2,  stp3, dimen, len);
}

Meter::Meter (Canvas* canvas, long hold, unsigned long dimen, Orientation o, int len,
		int clr0, int clr1, int clr2, int clr3,
		int clr4, int clr5, int clr6, int clr7,
		int clr8, int clr9,
		int bgc0, int bgc1,
		int bgh0, int bgh1,
		float stp0, float stp1,
		float stp2, float stp3,
		int styleflags
		)
	: Item (canvas)
	, pixheight(0)
	, pixwidth(0)
	, _styleflags(styleflags)
	, orientation(o)
	, hold_cnt(hold)
	, hold_state(0)
	, bright_hold(false)
	, current_level(0)
	, current_peak(0)
	, highlight(false)
{
	init (clr0, clr1, clr2, clr3, clr4, clr5, clr6, clr7, clr8, clr9, bgc0, bgc1, bgh0, bgh1, stp0, stp1, stp2,  stp3, dimen, len);
}

void
Meter::init (int clr0, int clr1, int clr2, int clr3,
             int clr4, int clr5, int clr6, int clr7,
             int clr8, int clr9,
             int bgc0, int bgc1,
             int bgh0, int bgh1,
             float stp0, float stp1,
             float stp2, float stp3,
             int dimen,
             int len)
{
	last_peak_rect.width = 0;
	last_peak_rect.height = 0;
	last_peak_rect.x = 0;
	last_peak_rect.y = 0;

	no_rgba_overlay = ! Glib::getenv("NO_METER_SHADE").empty();

	_clr[0] = clr0;
	_clr[1] = clr1;
	_clr[2] = clr2;
	_clr[3] = clr3;
	_clr[4] = clr4;
	_clr[5] = clr5;
	_clr[6] = clr6;
	_clr[7] = clr7;
	_clr[8] = clr8;
	_clr[9] = clr9;

	_bgc[0] = bgc0;
	_bgc[1] = bgc1;

	_bgh[0] = bgh0;
	_bgh[1] = bgh1;

	_stp[0] = stp0;
	_stp[1] = stp1;
	_stp[2] = stp2;
	_stp[3] = stp3;

	if (!len) {
		len = 250;
	}

	if (orientation == Vertical) {
		pixheight = len;
		pixwidth = dimen;
		fgpattern = vertical_meter_pattern (pixwidth + 2, pixheight + 2, _clr, _stp, _styleflags);
		bgpattern = vertical_background (pixwidth + 2, pixheight + 2, _bgc, false);
	} else {
		pixheight = dimen;
		pixwidth = len;
		fgpattern = horizontal_meter_pattern (pixwidth + 2, pixheight + 2, _clr, _stp, _styleflags);
		bgpattern = horizontal_background (pixwidth + 2, pixheight + 2, _bgc, false);
	}

	pixrect.height = pixheight;
	pixrect.x = 1;

	if (orientation == Vertical) {
		pixrect.width = pixwidth;
		pixrect.y = pixheight; /* bottom of meter, so essentially "show zero" */
	} else {
		pixrect.width = 0; /* right of meter, so essentially "show zero" */
		pixrect.y = 1;
	}
}

void
Meter::compute_bounding_box () const
{
	if (!_canvas) {
		_bounding_box = Rect ();
		bb_clean ();
		return;
	}

	Rect r (0, 0, pixwidth + 2, pixheight + 2);
	_bounding_box = r;
	bb_clean ();
}


Meter::~Meter ()
{
}

void
Meter::flush_pattern_cache () {
	hb_pattern_cache.clear();
	hm_pattern_cache.clear();
	vb_pattern_cache.clear();
	vm_pattern_cache.clear();
}

Cairo::RefPtr<Cairo::Pattern>
Meter::generate_meter_pattern (int width, int height, int *clr, float *stp, int styleflags, bool horiz)
{
	guint8 r,g,b,a;
	double knee;
	const double soft =  3.0 / (double) height;
	const double offs = -1.0 / (double) height;

	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, height);

	/*
	  Cairo coordinate space goes downwards as y value goes up, so invert
	  knee-based positions by using (1.0 - y)
	*/

	UINT_TO_RGBA (clr[9], &r, &g, &b, &a); // top/clip
	cairo_pattern_add_color_stop_rgb (pat, 0.0,
	                                  r/255.0, g/255.0, b/255.0);

	knee = offs + stp[3] / 115.0f; // -0dB

	UINT_TO_RGBA (clr[8], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee,
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[7], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee + soft,
	                                  r/255.0, g/255.0, b/255.0);

	knee = offs + stp[2]/ 115.0f; // -3dB || -2dB

	UINT_TO_RGBA (clr[6], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee,
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[5], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee + soft,
	                                  r/255.0, g/255.0, b/255.0);

	knee = offs + stp[1] / 115.0f; // -9dB

	UINT_TO_RGBA (clr[4], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee,
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[3], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee + soft,
	                                  r/255.0, g/255.0, b/255.0);

	knee = offs + stp[0] / 115.0f; // -18dB

	UINT_TO_RGBA (clr[2], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee,
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[1], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - knee + soft,
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[0], &r, &g, &b, &a); // bottom
	cairo_pattern_add_color_stop_rgb (pat, 1.0,
	                                  r/255.0, g/255.0, b/255.0);

	if ((styleflags & 1) && !no_rgba_overlay) {
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, width, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 0,   0.0, 0.0, 0.0, 0.15);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 0.4, 1.0, 1.0, 1.0, 0.05);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 1,   0.0, 0.0, 0.0, 0.25);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
		tc = cairo_create (surface);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);
		cairo_pattern_destroy (pat);

		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);
		cairo_pattern_destroy (shade_pattern);

		if (styleflags & 2) { // LED stripes
			cairo_save (tc);
			cairo_set_line_width(tc, 1.0);
			cairo_set_source_rgba(tc, .0, .0, .0, 0.4);
			//cairo_set_operator (tc, CAIRO_OPERATOR_SOURCE);
			for (int i = 0; float y = 0.5 + i * 2.0; ++i) {
				if (y >= height) {
					break;
				}
				cairo_move_to(tc, 0, y);
				cairo_line_to(tc, width, y);
				cairo_stroke (tc);
			}
			cairo_restore (tc);
		}

		pat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	if (horiz) {
		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
		tc = cairo_create (surface);

		cairo_matrix_t m;
		cairo_matrix_init_rotate (&m, -M_PI/2.0);
		cairo_matrix_translate (&m, -height, 0);
		cairo_pattern_set_matrix (pat, &m);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, height, width);
		cairo_fill (tc);
		cairo_pattern_destroy (pat);
		pat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}
	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));

	return p;
}


Cairo::RefPtr<Cairo::Pattern>
Meter::generate_meter_background (int width, int height, int *clr, bool shade, bool horiz)
{
	guint8 r0,g0,b0,r1,g1,b1,a;

	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, height);

	UINT_TO_RGBA (clr[0], &r0, &g0, &b0, &a);
	UINT_TO_RGBA (clr[1], &r1, &g1, &b1, &a);

	cairo_pattern_add_color_stop_rgb (pat, 0.0,
	                                  r1/255.0, g1/255.0, b1/255.0);

	cairo_pattern_add_color_stop_rgb (pat, 1.0,
	                                  r0/255.0, g0/255.0, b0/255.0);

	if (shade && !no_rgba_overlay) {
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, width, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1.0, 1.0, 1.0, 0.15);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 0.6, 0.0, 0.0, 0.0, 0.10);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 1.0, 1.0, 1.0, 0.20);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
		tc = cairo_create (surface);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);
		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);

		cairo_pattern_destroy (pat);
		cairo_pattern_destroy (shade_pattern);

		pat = cairo_pattern_create_for_surface (surface);

		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	if (horiz) {
		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
		tc = cairo_create (surface);

		cairo_matrix_t m;
		cairo_matrix_init_rotate (&m, -M_PI/2.0);
		cairo_matrix_translate (&m, -height, 0);
		cairo_pattern_set_matrix (pat, &m);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, height, width);
		cairo_fill (tc);
		cairo_pattern_destroy (pat);
		pat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
Meter::vertical_meter_pattern (int width, int height, int *clr, float *stp, int styleflags)
{
	height = max(height, min_pattern_metric_size);
	height = min(height, max_pattern_metric_size);

	const Pattern10MapKey key (width, height,
			stp[0], stp[1], stp[2], stp[3],
			clr[0], clr[1], clr[2], clr[3],
			clr[4], clr[5], clr[6], clr[7],
			clr[8], clr[9], styleflags);

	Pattern10Map::iterator i;
	if ((i = vm_pattern_cache.find (key)) != vm_pattern_cache.end()) {
		return i->second;
	}
	// TODO flush pattern cache if it gets too large

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (width, height, clr, stp, styleflags, false);
	vm_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
Meter::vertical_background (int width, int height, int *bgc, bool shade)
{
	height = max(height, min_pattern_metric_size);
	height = min(height, max_pattern_metric_size);
	height += 2;

	const PatternBgMapKey key (width, height, bgc[0], bgc[1], shade);
	PatternBgMap::iterator i;

	if ((i = vb_pattern_cache.find (key)) != vb_pattern_cache.end()) {
		return i->second;
	}
	// TODO flush pattern cache if it gets too large

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_background (width, height, bgc, shade, false);
	vb_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
Meter::horizontal_meter_pattern (int width, int height, int *clr, float *stp, int styleflags)
{
	width = max(width, min_pattern_metric_size);
	width = min(width, max_pattern_metric_size);

	const Pattern10MapKey key (width, height,
			stp[0], stp[1], stp[2], stp[3],
			clr[0], clr[1], clr[2], clr[3],
			clr[4], clr[5], clr[6], clr[7],
			clr[8], clr[9], styleflags);

	Pattern10Map::iterator i;
	if ((i = hm_pattern_cache.find (key)) != hm_pattern_cache.end()) {
		return i->second;
	}
	// TODO flush pattern cache if it gets too large

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (height, width, clr, stp, styleflags, true);

	hm_pattern_cache[key] = p;
	return p;
}

Cairo::RefPtr<Cairo::Pattern>
Meter::horizontal_background (int width, int height, int *bgc, bool shade)
{
	width = max(width, min_pattern_metric_size);
	width = min(width, max_pattern_metric_size);
	width += 2;

	const PatternBgMapKey key (width, height, bgc[0], bgc[1], shade);
	PatternBgMap::iterator i;
	if ((i = hb_pattern_cache.find (key)) != hb_pattern_cache.end()) {
		return i->second;
	}
	// TODO flush pattern cache if it gets too large

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_background (height, width, bgc, shade, true);

	hb_pattern_cache[key] = p;

	return p;
}

void
Meter::set_hold_count (long val)
{
	if (val < 1) {
		val = 1;
	}

	hold_cnt = val;
	hold_state = 0;
	current_peak = 0;

	redraw ();
}

void
Meter::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (orientation == Vertical) {
		return vertical_expose (area, context);
	} else {
		return horizontal_expose (area, context);
	}
}

void
Meter::vertical_expose (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	gint top_of_meter;
	Cairo::RectangleInt background;
	Cairo::RectangleInt area_r;

	/* convert expose area back to item coordinate space */
	Rect area2 = window_to_item (area);

	area_r.x = area2.x0;
	area_r.y = area2.y0;
	area_r.width = area2.width();
	area_r.height = area2.height();

	context->set_source_rgb (0, 0, 0); // black
	rounded_rectangle (context, 0, 0, pixwidth + 2, pixheight + 2, 2);
	context->stroke ();

	top_of_meter = (gint) floor (pixheight * current_level);

	/* reset the height & origin of the rect that needs to show the meter pattern
	 */
	pixrect.height = top_of_meter;

	/* X/cairo coordinates; y grows down so y origin of pixrect (pattern
	   fill area) is the TOP of the pattern area, which we compute like this:

	   - start at 1
           - go to bottom of meter (pixheight)
           - back up by current meter height (top_of_meter)
	*/
	pixrect.y = 1 + pixheight - top_of_meter;

	background.x = 1;
	background.y = 1;
	background.width = pixrect.width;
	background.height = pixheight - top_of_meter;

	/* translate so that item coordinates match window coordinates */
	Duple origin (0, 0);
	origin = item_to_window (origin);
	context->translate (origin.x, origin.y);

	Cairo::RefPtr<Cairo::Region> r1 = Cairo::Region::create (area_r);
	r1->intersect (background);

	if (!r1->empty()) {
		Cairo::RectangleInt i (r1->get_extents ());
		context->set_source (bgpattern);
		context->rectangle (i.x, i.y, i.width, i.height);
		context->fill ();
	}

	Cairo::RefPtr<Cairo::Region> r2 = Cairo::Region::create (area_r);
	r2->intersect (pixrect);

	if (!r2->empty()) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		Cairo::RectangleInt i (r2->get_extents ());
		context->set_source (fgpattern);
		context->rectangle (i.x, i.y, i.width, i.height);
		context->fill ();
	}

	// draw peak bar

	if (hold_state) {
		last_peak_rect.x = 1;
		last_peak_rect.width = pixwidth;
		last_peak_rect.y = max(1, 1 + pixheight - (int) floor (pixheight * current_peak));
		if (_styleflags & 2) { // LED stripes
			last_peak_rect.y = max(0, (last_peak_rect.y & (~1)));
		}
		if (bright_hold || (_styleflags & 2)) {
			last_peak_rect.height = max(0, min(3, pixheight - last_peak_rect.y - 1 ));
		} else {
			last_peak_rect.height = max(0, min(2, pixheight - last_peak_rect.y - 1 ));
		}

		context->set_source (fgpattern);
		context->rectangle (last_peak_rect.x, last_peak_rect.y, last_peak_rect.width, last_peak_rect.height);

		if (bright_hold && !no_rgba_overlay) {
			context->fill_preserve ();
			context->set_source_rgba (1.0, 1.0, 1.0, 0.3);
		}
		context->fill ();

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	context->translate (-origin.x, -origin.y);
}

void
Meter::horizontal_expose (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	gint right_of_meter;
	Cairo::RectangleInt background;
	Cairo::RectangleInt area_r;

	/* convert expose area back to item coordinate space */
	Rect area2 = window_to_item (area);

	/* create a Cairo object so that we can use intersect and Region */
	area_r.x = area2.x0;
	area_r.y = area2.y0;
	area_r.width = area2.width();
	area_r.height = area2.height();

	/* draw the edge (rounded corners) */
	context->set_source_rgb (0, 0, 0); // black
	rounded_rectangle (context, 0, 0, pixwidth + 2, pixheight + 2, 2);
	context->stroke ();

	/* horizontal meter extends from left to right. Compute the right edge */
	right_of_meter = (gint) floor (pixwidth * current_level);

	/* reset the width the rect that needs to show the pattern of the meter */
	pixrect.width = right_of_meter;

	/* compute a rect for the part of the meter that is all background */
	background.x = 1 + right_of_meter;
	background.y = 1;
	background.width = pixwidth - right_of_meter;
	background.height = pixheight;

	/* translate so that item coordinates match window coordinates */
	Duple origin (0, 0);
	origin = item_to_window (origin);
	context->translate (origin.x, origin.y);

	Cairo::RefPtr<Cairo::Region> r;

	r = Cairo::Region::create (area_r);
	r->intersect (background);

	if (!r->empty()) {
		/* draw the background part */
		Cairo::RectangleInt i (r->get_extents ());
		context->set_source (bgpattern);
		context->rectangle (i.x, i.y, i.width, i.height);
		context->fill ();

	}

	r  = Cairo::Region::create (area_r);
	r->intersect (pixrect);

	if (!r->empty()) {
		// draw the part of the meter image that we need.
		Cairo::RectangleInt i (r->get_extents ());
		Duple d (i.x, i.y);
		context->set_source (fgpattern);
		context->rectangle (i.x, i.y, i.width, i.height);
		context->fill ();
	}

	// draw peak bar

	if (hold_state) {
		last_peak_rect.y = 1;
		last_peak_rect.height = pixheight;
		const int xpos = floor (pixwidth * current_peak);
		if (bright_hold || (_styleflags & 2)) {
			last_peak_rect.width = min(3, xpos );
		} else {
			last_peak_rect.width = min(2, xpos );
		}
		last_peak_rect.x = 1 + max(0, xpos - last_peak_rect.width);

		context->set_source (fgpattern);
		context->rectangle (last_peak_rect.x, last_peak_rect.y, last_peak_rect.width, last_peak_rect.height);

		if (bright_hold && !no_rgba_overlay) {
			context->fill_preserve ();
			context->set_source_rgba (1.0, 1.0, 1.0, 0.3);
		}
		context->fill ();

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	context->translate (-origin.x, -origin.y);
}

void
Meter::set (float lvl, float peak)
{
	float old_level = current_level;
	float old_peak = current_peak;

	if (pixwidth <= 0 || pixheight <=0) return;

	if (peak == -1) {
		if (lvl >= current_peak && lvl > 0) {
			current_peak = lvl;
			hold_state = hold_cnt;
		}

		if (hold_state > 0) {
			if (--hold_state == 0) {
				current_peak = lvl;
			}
		}
		bright_hold = false;
	} else {
		current_peak = peak;
		hold_state = 1;
		bright_hold = true;
	}

	current_level = lvl;

	const float pixscale = (orientation == Vertical) ? pixheight : pixwidth;
#define PIX(X) floor(pixscale * (X))
	if (PIX(current_level) == PIX(old_level) && PIX(current_peak) == PIX(old_peak) && (hold_state == 0 || peak != -1)) {
		return;
	}

	if (orientation == Vertical) {
		queue_vertical_redraw (old_level);
	} else {
		queue_horizontal_redraw (old_level);
	}
}

void
Meter::queue_vertical_redraw (float old_level)
{
	Cairo::RectangleInt rect;

	gint new_height = (gint) floor (pixheight * current_level);

	/* this is the nominal area that needs to be filled by the meter
	 * pattern
	 */

	rect.x = 1;
	rect.width = pixwidth;

	/* compute new top of meter (rect.y) by starting at one (border
	 * offset), go down the full height of the meter (X/Cairo coordinates
	 * grow down) to get to the bottom coordinate, then back up by the
	 * height of the patterned area.
	 */
	rect.y = 1 + pixheight - new_height;
	/* remember: height extends DOWN thanks to X/Cairo */
	rect.height = new_height;

	/* now lets optimize redrawing by figuring out which part needs to be
	   actually redrawn (i.e. re-use the last drawn state).
	*/

	if (current_level > old_level) {
		/* filled area got taller, just draw the new section */

		/* rect.y (new y origin) is smaller or equal to pixrect.y (old
		 * y origin) because the top of the meter is higher (X/Cairo:
		 * coordinates grow down). 
		 *
		 * Leave rect.y alone, and recompute the height to be just the
		 * difference between the new bottom and the top of the previous
		 * pattern area.
		 *
		 * The old pattern area extended DOWN from pixrect.y to
		 * pixrect.y + pixrect.height.
		 *
		 * The new pattern area extends DOWN from rect.y to 
		 * rect.y + rect.height
		 *
		 * The area needing to be drawn is the difference between the
		 * old top (pixrect.y) and the new top (rect.y)
		*/
		rect.height = pixrect.y - rect.y;
	} else {
		/* it got smaller, compute the difference */
		/* rect.y becomes old.y (the smaller value) */
		rect.y = pixrect.y;
		/* rect.height is the old.y (smaller) minus the new.y (larger)
		*/
		rect.height = pixrect.height - rect.height;
	}

	Cairo::RefPtr<Cairo::Region> region;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = Cairo::Region::create (rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = Cairo::Region::create ();
			queue = true;
		}
		region->do_union (last_peak_rect);
	}

	if (hold_state && current_peak > 0) {
		if (!queue) {
			region = Cairo::Region::create ();
			queue = true;
		}
		rect.x = 1;
		rect.y = max(1, 1 + pixheight - (int) floor (pixheight * current_peak));
		if (_styleflags & 2) { // LED stripes
			rect.y = max(0, (rect.y & (~1)));
		}
		if (bright_hold || (_styleflags & 2)) {
			rect.height = max(0, min(3, pixheight - last_peak_rect.y -1 ));
		} else {
			rect.height = max(0, min(2, pixheight - last_peak_rect.y -1 ));
		}
		rect.width = pixwidth;
		region->do_union (rect);
	}

	if (queue) {
		if (visible() && _bounding_box && _canvas) {
			Cairo::RectangleInt iri = region->get_extents();
			Rect ir (iri.x, iri.y, iri.x + iri.width, iri.y + iri.height);
			_canvas->request_redraw (item_to_window (ir));
		}
	}
}

void
Meter::queue_horizontal_redraw (float old_level)
{
	Cairo::RectangleInt rect;

	gint new_right = (gint) floor (pixwidth * current_level);

	rect.height = pixheight;
	rect.y = 1;

	if (current_level > old_level) {
		rect.x = 1 + pixrect.width;
		/* colored/pixbuf got larger, just draw the new section */
		rect.width = new_right - pixrect.width;
	} else {
		/* it got smaller, compute the difference */
		rect.x = 1 + new_right;
		/* rect.height is the old.x (smaller) minus the new.x (larger) */
		rect.width = pixrect.width - new_right;
	}

	Cairo::RefPtr<Cairo::Region> region;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = Cairo::Region::create (rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = Cairo::Region::create ();
			queue = true;
		}
		region->do_union (last_peak_rect);
	}

	if (hold_state && current_peak > 0) {
		if (!queue) {
			region = Cairo::Region::create ();
			queue = true;
		}
		rect.y = 1;
		rect.height = pixheight;
		const int xpos = floor (pixwidth * current_peak);
		if (bright_hold || (_styleflags & 2)) {
			rect.width = min(3, xpos);
		} else {
			rect.width = min(2, xpos);
		}
		rect.x = 1 + max(0, xpos - rect.width);
		region->do_union (rect);
	}

	if (queue) {
		if (visible() && _bounding_box && _canvas) {
			Cairo::RectangleInt iri = region->get_extents();
			Rect ir (iri.x, iri.y, iri.x + iri.width, iri.y + iri.height);
			_canvas->request_redraw (item_to_window (ir));
		}
	}
}

void
Meter::set_highlight (bool onoff)
{
	if (highlight == onoff) {
		return;
	}
	highlight = onoff;
	if (orientation == Vertical) {
		bgpattern = vertical_background (pixwidth + 2, pixheight + 2, highlight ? _bgh : _bgc, highlight);
	} else {
		bgpattern = horizontal_background (pixwidth + 2, pixheight + 2, highlight ? _bgh : _bgc, highlight);
	}
	redraw ();
}

void
Meter::clear ()
{
	current_level = 0;
	current_peak = 0;
	hold_state = 0;
	redraw ();
}
