/*
    Copyright (C) 2003-2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#include <stdlib.h>

#include <gdkmm/rectangle.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/utils.h>

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

int FastMeter::min_pattern_metric_size = 16;
int FastMeter::max_pattern_metric_size = 1024;
bool FastMeter::no_rgba_overlay = false;

FastMeter::Pattern10Map FastMeter::vm_pattern_cache;
FastMeter::PatternBgMap FastMeter::vb_pattern_cache;

FastMeter::Pattern10Map FastMeter::hm_pattern_cache;
FastMeter::PatternBgMap FastMeter::hb_pattern_cache;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o, int len,
		int clr0, int clr1, int clr2, int clr3,
		int clr4, int clr5, int clr6, int clr7,
		int clr8, int clr9,
		int bgc0, int bgc1,
		int bgh0, int bgh1,
		float stp0, float stp1,
		float stp2, float stp3,
		int styleflags
		)
	: pixheight(0)
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

	set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);

	pixrect.x = 1;
	pixrect.y = 1;

	if (!len) {
		len = 250;
	}
	if (orientation == Vertical) {
		pixheight = len;
		pixwidth = dimen;
		fgpattern = request_vertical_meter(pixwidth + 2, pixheight + 2, _clr, _stp, _styleflags);
		bgpattern = request_vertical_background (pixwidth + 2, pixheight + 2, _bgc, false);

	} else {
		pixheight = dimen;
		pixwidth = len;
		fgpattern = request_horizontal_meter(pixwidth + 2, pixheight + 2, _clr, _stp, _styleflags);
		bgpattern = request_horizontal_background (pixwidth + 2, pixheight + 2, _bgc, false);
	}

	pixrect.width = pixwidth;
	pixrect.height = pixheight;

	request_width = pixrect.width + 2;
	request_height= pixrect.height + 2;

	clear ();
}

FastMeter::~FastMeter ()
{
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::generate_meter_pattern (
		int width, int height, int *clr, float *stp, int styleflags, bool horiz)
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
			for (float y=0.5; y < height; y+= 2.0) {
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
FastMeter::generate_meter_background (
		int width, int height, int *clr, bool shade, bool horiz)
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
FastMeter::request_vertical_meter(
		int width, int height, int *clr, float *stp, int styleflags)
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

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (
		width, height, clr, stp, styleflags, false);
	vm_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_vertical_background(
		int width, int height, int *bgc, bool shade)
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

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_background (
		width, height, bgc, shade, false);
	vb_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_horizontal_meter(
		int width, int height, int *clr, float *stp, int styleflags)
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

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (
		height, width, clr, stp, styleflags, true);

	hm_pattern_cache[key] = p;
	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_horizontal_background(
		int width, int height, int *bgc, bool shade)
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

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_background (
		height, width, bgc, shade, true);

	hb_pattern_cache[key] = p;

	return p;
}



void
FastMeter::set_hold_count (long val)
{
	if (val < 1) {
		val = 1;
	}

	hold_cnt = val;
	hold_state = 0;
	current_peak = 0;

	queue_draw ();
}

void
FastMeter::on_size_request (GtkRequisition* req)
{
	if (orientation == Vertical) {
		vertical_size_request (req);
	} else {
		horizontal_size_request (req);
	}
}

void
FastMeter::vertical_size_request (GtkRequisition* req)
{
	req->height = request_height;
	req->height = max(req->height, min_pattern_metric_size);
	req->height = min(req->height, max_pattern_metric_size);
	req->height += 2;

	req->width  = request_width;
}

void
FastMeter::horizontal_size_request (GtkRequisition* req)
{
	req->width = request_width;
	req->width = max(req->width, min_pattern_metric_size);
	req->width = min(req->width, max_pattern_metric_size);
	req->width += 2;

	req->height  = request_height;
}

void
FastMeter::on_size_allocate (Gtk::Allocation &alloc)
{
	if (orientation == Vertical) {
		vertical_size_allocate (alloc);
	} else {
		horizontal_size_allocate (alloc);
	}
	queue_draw ();
}

void
FastMeter::vertical_size_allocate (Gtk::Allocation &alloc)
{
	if (alloc.get_width() != request_width) {
		alloc.set_width (request_width);
	}

	int h = alloc.get_height();
	h = max (h, min_pattern_metric_size + 2);
	h = min (h, max_pattern_metric_size + 2);

	if (h != alloc.get_height()) {
		alloc.set_height (h);
	}

	if (pixheight != h) {
		fgpattern = request_vertical_meter (request_width, h, _clr, _stp, _styleflags);
		bgpattern = request_vertical_background (request_width, h, highlight ? _bgh : _bgc, highlight);
		pixheight = h - 2;
		pixwidth  = request_width - 2;
	}

	DrawingArea::on_size_allocate (alloc);
}

void
FastMeter::horizontal_size_allocate (Gtk::Allocation &alloc)
{
	if (alloc.get_height() != request_height) {
		alloc.set_height (request_height);
	}

	int w = alloc.get_width();
	w = max (w, min_pattern_metric_size + 2);
	w = min (w, max_pattern_metric_size + 2);

	if (w != alloc.get_width()) {
		alloc.set_width (w);
	}

	if (pixwidth != w) {
		fgpattern = request_horizontal_meter (w, request_height, _clr, _stp, _styleflags);
		bgpattern = request_horizontal_background (w, request_height, highlight ? _bgh : _bgc, highlight);
		pixwidth = w - 2;
		pixheight  = request_height - 2;
	}

	DrawingArea::on_size_allocate (alloc);
}

bool
FastMeter::on_expose_event (GdkEventExpose* ev)
{
	if (orientation == Vertical) {
		return vertical_expose (ev);
	} else {
		return horizontal_expose (ev);
	}
}

bool
FastMeter::vertical_expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win = get_window ();
	gint top_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_source_rgb (cr, 0, 0, 0); // black
	rounded_rectangle (cr, 0, 0, pixwidth + 2, pixheight + 2, 2);
	cairo_stroke (cr);

	top_of_meter = (gint) floor (pixheight * current_level);

	/* reset the height & origin of the rect that needs to show the pixbuf
	 */

	pixrect.height = top_of_meter;
	pixrect.y = 1 + pixheight - top_of_meter;

	background.x = 1;
	background.y = 1;
	background.width = pixrect.width;
	background.height = pixheight - top_of_meter;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		cairo_set_source (cr, bgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	// draw peak bar

	if (hold_state) {
		last_peak_rect.x = 1;
		last_peak_rect.width = pixwidth;
		last_peak_rect.y = max(1, 1 + pixheight - (gint) floor (pixheight * current_peak));
		if (bright_hold || (_styleflags & 2)) {
			last_peak_rect.height = max(0, min(3, pixheight - last_peak_rect.y - 1 ));
		} else {
			last_peak_rect.height = max(0, min(2, pixheight - last_peak_rect.y - 1 ));
		}

		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, last_peak_rect.x, last_peak_rect.y, last_peak_rect.width, last_peak_rect.height);

		if (bright_hold && !no_rgba_overlay) {
			cairo_fill_preserve (cr);
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.3);
		}
		cairo_fill (cr);

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	cairo_destroy (cr);

	return TRUE;
}

bool
FastMeter::horizontal_expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win = get_window ();
	gint right_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_source_rgb (cr, 0, 0, 0); // black
	rounded_rectangle (cr, 0, 0, pixwidth + 2, pixheight + 2, 2);
	cairo_stroke (cr);

	right_of_meter = (gint) floor (pixwidth * current_level);

	/* reset the height & origin of the rect that needs to show the pixbuf
	 */

	pixrect.width = right_of_meter;

	background.x = 1 + right_of_meter;
	background.y = 1;
	background.width = pixwidth - right_of_meter;
	background.height = pixheight;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		cairo_set_source (cr, bgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
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

		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, last_peak_rect.x, last_peak_rect.y, last_peak_rect.width, last_peak_rect.height);

		if (bright_hold && !no_rgba_overlay) {
			cairo_fill_preserve (cr);
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.3);
		}
		cairo_fill (cr);

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	cairo_destroy (cr);

	return TRUE;
}

void
FastMeter::set (float lvl, float peak)
{
	float old_level = current_level;
	float old_peak = current_peak;

	if (pixwidth <= 0 || pixheight <=0) return;

	if (peak == -1) {
		if (lvl >= current_peak) {
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

	if (current_level == old_level && current_peak == old_peak && (hold_state == 0 || peak != -1)) {
		return;
	}

	Glib::RefPtr<Gdk::Window> win;

	if ((win = get_window()) == 0) {
		queue_draw ();
		return;
	}

	if (orientation == Vertical) {
		queue_vertical_redraw (win, old_level);
	} else {
		queue_horizontal_redraw (win, old_level);
	}
}

void
FastMeter::queue_vertical_redraw (const Glib::RefPtr<Gdk::Window>& win, float old_level)
{
	GdkRectangle rect;

	gint new_top = (gint) floor (pixheight * current_level);

	rect.x = 1;
	rect.width = pixwidth;
	rect.height = new_top;
	rect.y = 1 + pixheight - new_top;

	if (current_level > old_level) {
		/* colored/pixbuf got larger, just draw the new section */
		/* rect.y stays where it is because of X coordinates */
		/* height of invalidated area is between new.y (smaller) and old.y
		   (larger).
		   X coordinates just make my brain hurt.
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

	GdkRegion* region = 0;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = gdk_region_rectangle (&rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = gdk_region_new ();
			queue = true;
		}
		gdk_region_union_with_rect (region, &last_peak_rect);
	}

	if (hold_state && current_peak > 0) {
		if (!queue) {
			region = gdk_region_new ();
			queue = true;
		}
		rect.x = 1;
		rect.y = max(1, 1 + pixheight - (gint) floor (pixheight * current_peak));
		if (bright_hold || (_styleflags & 2)) {
			rect.height = max(0, min(3, pixheight - last_peak_rect.y -1 ));
		} else {
			rect.height = max(0, min(2, pixheight - last_peak_rect.y -1 ));
		}
		rect.width = pixwidth;
		gdk_region_union_with_rect (region, &rect);
	}

	if (queue) {
		gdk_window_invalidate_region (win->gobj(), region, true);
	}
	if (region) {
		gdk_region_destroy(region);
		region = 0;
	}
}

void
FastMeter::queue_horizontal_redraw (const Glib::RefPtr<Gdk::Window>& win, float old_level)
{
	GdkRectangle rect;

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

	GdkRegion* region = 0;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = gdk_region_rectangle (&rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = gdk_region_new ();
			queue = true;
		}
		gdk_region_union_with_rect (region, &last_peak_rect);
	}

	if (hold_state && current_peak > 0) {
		if (!queue) {
			region = gdk_region_new ();
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
		gdk_region_union_with_rect (region, &rect);
	}

	if (queue) {
		gdk_window_invalidate_region (win->gobj(), region, true);
	}
	if (region) {
		gdk_region_destroy(region);
		region = 0;
	}
}

void
FastMeter::set_highlight (bool onoff)
{
	if (highlight == onoff) {
		return;
	}
	highlight = onoff;
	if (orientation == Vertical) {
		bgpattern = request_vertical_background (pixwidth + 2, pixheight + 2, highlight ? _bgh : _bgc, highlight);
	} else {
		bgpattern = request_horizontal_background (pixwidth + 2, pixheight + 2, highlight ? _bgh : _bgc, highlight);
	}
	queue_draw ();
}

void
FastMeter::clear ()
{
	current_level = 0;
	current_peak = 0;
	hold_state = 0;
	queue_draw ();
}
