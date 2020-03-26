/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include <math.h> // M_PI
#include <assert.h>
#include <algorithm> // std:min

#include "gtkmm2ext/colors.h"
#include "widgets/ardour_icon.h"

using namespace ArdourWidgets::ArdourIcon;

/* general style info:
 *
 * - geometry: icons should be centered, spanning
 *   wh = std::min (width * .5, height *.5) * .55;
 *
 * - all shapes should have a contrasting outline
 *   (usually white foreground, black outline)
 */

#define DEFAULT_LINE_WIDTH ceil (std::min (width, height) * .035)

#define OUTLINEWIDTH 1.5 // px

#define VECTORICONSTROKEFILL(fillalpha)              \
  cairo_set_line_width (cr, OUTLINEWIDTH);           \
  cairo_set_source_rgba (cr, 0, 0, 0, 1.0);          \
  cairo_stroke_preserve (cr);                        \
  cairo_set_source_rgba (cr, 1, 1, 1, (fillalpha));  \
  cairo_fill (cr);

#define VECTORICONSTROKEOUTLINE(LW, color)           \
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);     \
  cairo_set_line_width (cr, (LW) + OUTLINEWIDTH);    \
  ardour_icon_set_source_inv_rgba (cr, color);       \
  cairo_stroke_preserve (cr);                        \
  Gtkmm2ext::set_source_rgba (cr, color);            \
  cairo_set_line_width (cr, (LW));                   \
  cairo_stroke (cr);

#define VECTORICONSTROKE(LW, color)                  \
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);     \
  Gtkmm2ext::set_source_rgba (cr, color);            \
  cairo_set_line_width (cr, (LW));                   \
  cairo_stroke (cr);


/** inverse color */
static void ardour_icon_set_source_inv_rgba (cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba (cr,
			1.0 - ((color >> 24) & 0xff) / 255.0,
			1.0 - ((color >> 16) & 0xff) / 255.0,
			1.0 - ((color >>  8) & 0xff) / 255.0,
			((color >>  0) & 0xff) / 255.0
			);
}

/*****************************************************************************
 * Tool Icons.
 * Foreground is always white, compatible with small un-blurred rendering.
 */

/** internal edit icon */
static void icon_tool_content (cairo_t *cr, const int width, const int height) {
#define EM_POINT(X,Y) round (x + (X) * em) + .5, round (y + (Y) * em) + .5

		const double x  = width * .5;
		const double y  = height * .5;
		const double em = std::min (x, y) * .1; // 1px at 20x20

		// draw dot outlines (control-points)
		cairo_move_to (cr, EM_POINT(-6.0,  0.0));
		cairo_close_path (cr);
		cairo_move_to (cr, EM_POINT(-2.5,  4.0));
		cairo_close_path (cr);
		cairo_move_to (cr, EM_POINT( 5.0, -5.0));
		cairo_close_path (cr);

		cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
		ardour_icon_set_source_inv_rgba (cr, 0xffffffff);
		cairo_set_line_width (cr, 3 * em + OUTLINEWIDTH);
		cairo_stroke (cr);

		// "midi note" lines
		cairo_move_to (cr, EM_POINT(-7.0, -5.0));
		cairo_line_to (cr, EM_POINT( 0.0, -5.0));

		cairo_move_to (cr, EM_POINT( 2.0,  4.0));
		cairo_line_to (cr, EM_POINT( 6.0,  4.0));

		// automation line (connect control-points)
		cairo_move_to (cr, EM_POINT(-6.0,  0.0));
		cairo_line_to (cr, EM_POINT(-2.5,  4.0));
		cairo_line_to (cr, EM_POINT( 5.0, -5.0));

		cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
		VECTORICONSTROKEOUTLINE(1 * em, 0xffffffff);

		// remove automation line outline at control-points
		cairo_move_to (cr, EM_POINT(-6.0,  0.0));
		cairo_close_path (cr);
		cairo_move_to (cr, EM_POINT(-2.5,  4.0));
		cairo_close_path (cr);
		cairo_move_to (cr, EM_POINT( 5.0, -5.0));
		cairo_close_path (cr);

		Gtkmm2ext::set_source_rgba (cr, 0xffffffff);
		cairo_set_line_width (cr, 3 * em);
		cairo_stroke (cr);
#undef EM_POINT
}

/** range tool |<->| */
static void icon_tool_range (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double wh = std::min (x, y) * .55;
	const double ar = wh * .6; // arrow

	const double bw = ceil (wh) - .5;
	const double y0 = ceil (y);
	const double ym = rint (y0 - wh * .1) + .5; // arrow-horizontal; slightly to the top, on a px
	const double x0 = rint (x) - bw; // left arrow tip
	const double x1 = rint (x) + bw; // right arrow tip

	// left and right box
	cairo_move_to (cr, x0, y0 - bw);
	cairo_line_to (cr, x0, y0 + bw);
	cairo_move_to (cr, x1, y0 - bw);
	cairo_line_to (cr, x1, y0 + bw);

	// arrows
	cairo_move_to (cr, x0 + ar, ym - ar);
	cairo_line_to (cr, x0 + .5, ym);
	cairo_line_to (cr, x0 + ar, ym + ar);

	cairo_move_to (cr, x1 - ar, ym - ar);
	cairo_line_to (cr, x1 - .5, ym);
	cairo_line_to (cr, x1 - ar, ym + ar);

	// line connecting the arrows
	cairo_move_to (cr, x0, ym);
	cairo_line_to (cr, x1, ym);
	VECTORICONSTROKEOUTLINE(DEFAULT_LINE_WIDTH, 0xffffffff);
}

/** Grab/Object tool - 6x8em "hand", with 'em' wide index finger. */
static void icon_tool_grab (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double em = std::min (x, y) * .15; // 1.5px at 20x20

#define EM_POINT(X,Y) x + (X) * em, y + (Y) * em

	// wrist
	cairo_move_to (cr, EM_POINT( 2.0,  4.0));
	cairo_line_to (cr, EM_POINT(-1.5,  4.0));
	cairo_line_to (cr, EM_POINT(-2.5,  2.0));
	// thumb
	cairo_line_to (cr, EM_POINT(-3.0,  1.0));

	// index finger
	cairo_line_to (cr, EM_POINT(-2.0,  0.0));
	cairo_line_to (cr, EM_POINT(-2.1, -4.0));
	cairo_line_to (cr, EM_POINT(-1.5, -4.5));
	cairo_line_to (cr, EM_POINT(-1.1, -4.0));
	cairo_line_to (cr, EM_POINT(-1.0,  0.1));

	// middle finger knuckle
	cairo_line_to (cr, EM_POINT(-0.6,  0.3));
	cairo_line_to (cr, EM_POINT(-0.3,  0.0));
	cairo_line_to (cr, EM_POINT(-0.2, -0.2));
	cairo_line_to (cr, EM_POINT( 0.1, -0.3));
	cairo_line_to (cr, EM_POINT( 0.4, -0.2));
	cairo_line_to (cr, EM_POINT( 0.5,  0.1));

	// ring finger knuckle
	cairo_line_to (cr, EM_POINT( 0.8,  0.4));
	cairo_line_to (cr, EM_POINT( 1.1,  0.2));
	cairo_line_to (cr, EM_POINT( 1.2,  0.0));
	cairo_line_to (cr, EM_POINT( 1.5, -0.1));
	cairo_line_to (cr, EM_POINT( 1.8,  0.0));
	cairo_line_to (cr, EM_POINT( 1.9,  0.4));

	// pinky
	cairo_line_to (cr, EM_POINT( 2.0,  0.6));
	cairo_line_to (cr, EM_POINT( 2.4,  0.4));
	cairo_line_to (cr, EM_POINT( 2.8,  0.5));
	cairo_line_to (cr, EM_POINT( 3.0,  1.0));

	// wrist
	cairo_line_to (cr, EM_POINT( 3.0,  1.5));
	cairo_line_to (cr, EM_POINT( 2.0,  4.0));

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	VECTORICONSTROKEFILL(1.0);
#undef EM_POINT
}

/** cut icon - scissors */
static void icon_tool_cut (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double em = std::min (x, y) * .1; // 1px at 20x20

#define EM_POINT(X,Y) x + (X) * em, y + (Y) * em

	cairo_save (cr);
	cairo_translate (cr, EM_POINT(4, -3));
	cairo_scale (cr, 1.6, 1.0); // ellipse
	cairo_arc (cr, 0., 0., 1.5 * em, 0., 2 * M_PI);
	cairo_restore (cr);

	cairo_move_to (cr, EM_POINT(-6.0,  2.5));
	cairo_line_to (cr, EM_POINT( 5.5, -2.0));

	cairo_move_to (cr, EM_POINT(-6.0, -2.5));
	cairo_line_to (cr, EM_POINT( 5.5,  2.0));

	cairo_save (cr);
	cairo_translate (cr, EM_POINT(4,  3));
	cairo_scale (cr, 1.6, 1.0); // ellipse
	cairo_arc (cr, 0., 0., 1.5 * em, 0., 2 * M_PI);
	cairo_restore (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

	VECTORICONSTROKEOUTLINE (1.5 * em, 0xffffffff);
#undef EM_POINT
}

/** time stretch icon */
static void icon_tool_stretch (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double wh = std::min (x, y) * .55;

	const double y0 = ceil (y);
	const double bw = rint (wh);
	const double lw = rint (wh / 3.0) / 2.0;
	const double x0 = rint (x + lw) + .5;

	// box indication region
	cairo_rectangle (cr, x0 - lw - bw - .5, y0 - bw, lw + bw, 2 * bw);
	VECTORICONSTROKEFILL (0.75);

	cairo_set_line_width (cr, 1.0);

	// inside/left arrow
	cairo_move_to (cr, x0,          y);
	cairo_line_to (cr, x0 - lw * 2, y);
	cairo_line_to (cr, x0 - lw * 2, y - lw * 3.5);
	cairo_line_to (cr, x0 - lw * 6, y);
	cairo_line_to (cr, x0 - lw * 2, y + lw * 3.5);
	cairo_line_to (cr, x0 - lw * 2, y);

	cairo_set_source_rgba (cr, 0, 0, 0, .5);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);

	// outside/right arrow
	cairo_move_to (cr, x0,          y);
	cairo_line_to (cr, x0 + lw * 2, y);
	cairo_line_to (cr, x0 + lw * 2, y - lw * 4);
	cairo_line_to (cr, x0 + lw * 6, y);
	cairo_line_to (cr, x0 + lw * 2, y + lw * 4);
	cairo_line_to (cr, x0 + lw * 2, y);

	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
	cairo_fill (cr);
}

/** audition - small speaker with sound-waves*/
static void icon_tool_audition (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double em = std::min (x, y) * .1; // 1px at 20x20

#define EM_POINT(X,Y) x + (X) * em, y + (Y) * em

	cairo_move_to (cr, EM_POINT(-7.0, -2.0));
	cairo_line_to (cr, EM_POINT(-7.0,  2.0));
	cairo_line_to (cr, EM_POINT(-6.0,  3.0));
	cairo_line_to (cr, EM_POINT(-3.0,  3.0));
	cairo_line_to (cr, EM_POINT( 2.0,  6.0));
	cairo_line_to (cr, EM_POINT( 2.0, -6.0));
	cairo_line_to (cr, EM_POINT(-3.0, -3.0));
	cairo_line_to (cr, EM_POINT(-6.0, -3.0));
	cairo_close_path (cr);

	cairo_pattern_t *speaker;
	speaker = cairo_pattern_create_linear (EM_POINT(0, -3.0), EM_POINT(0, 3.0));
	cairo_pattern_add_color_stop_rgba (speaker, 0.0,  0.8, 0.8, 0.8, 1.0);
	cairo_pattern_add_color_stop_rgba (speaker, 0.25, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba (speaker, 1.0,  0.6, 0.6, 0.6, 1.0);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 1.5);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_stroke_preserve (cr);
	cairo_set_source (cr, speaker);
	cairo_fill (cr);
	cairo_pattern_destroy (speaker);

	// TODO use a slight curve
	cairo_move_to (cr, EM_POINT(-3.0, -3.0));
	cairo_line_to (cr, EM_POINT(-3.5,  0.0));
	cairo_line_to (cr, EM_POINT(-3.0,  3.0));
	cairo_set_source_rgba (cr, 0, 0, 0, 0.7);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);

	cairo_save (cr);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgba (cr, 1, 1, 1, 1);

	cairo_translate (cr, EM_POINT (4.0, 0));
	cairo_scale (cr, 0.8, 1.25); // ellipse

	cairo_arc (cr, 0, 0, 4 * em, -.5 * M_PI, .5 * M_PI);
	cairo_set_line_width (cr, .8 * em);
	cairo_stroke (cr);

	cairo_arc (cr, 0, 0, 2 * em, -.5 * M_PI, .5 * M_PI);
	cairo_set_line_width (cr, .5 * em);
	cairo_stroke (cr);
	cairo_restore (cr);
#undef EM_POINT
}

/** pen top-left to bottom right */
static void icon_tool_draw (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double em = std::min (x, y) * .1; // 1px at 20x20

#define EM_POINT(X,Y) x + (X) * em, y + (Y) * em

	// pen [6,-5] to [-3, 3]
	// y = -8 * x / 9 + 1/3

	// top-right end
	cairo_move_to (cr, EM_POINT( 5.0, -6.11));
	cairo_line_to (cr, EM_POINT( 6.4, -5.35)); // todo round properly.
	cairo_line_to (cr, EM_POINT( 7.0, -3.88));

	// bottom-left w/tip
	cairo_line_to (cr, EM_POINT(-2.0,  4.11));
	cairo_line_to (cr, EM_POINT(-6.0,  5.66)); // pen tip
	cairo_line_to (cr, EM_POINT(-4.0,  1.88));
	cairo_close_path (cr);

	cairo_pattern_t *pen;
	pen = cairo_pattern_create_linear (EM_POINT(-3.0, -6.0), EM_POINT(6.0, 4.0));
	cairo_pattern_add_color_stop_rgba (pen, 0.4, 0.6, 0.6, 0.6, 1.0);
	cairo_pattern_add_color_stop_rgba (pen, 0.5, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba (pen, 0.6, 0.1, 0.1, 0.1, 1.0);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, em + .5);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_stroke_preserve (cr);
	cairo_set_source (cr, pen);
	cairo_fill (cr);

	// separate the tip
	cairo_move_to (cr, EM_POINT(-2.0,  4.11));
	cairo_line_to (cr, EM_POINT(-3.0,  2.8)); // slight curve [-3,3]
	cairo_line_to (cr, EM_POINT(-4.0,  2.0));
	cairo_set_line_width (cr, OUTLINEWIDTH);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_stroke (cr);

	// pen tip
	cairo_move_to (cr, EM_POINT(-5.0, 3.9));
	cairo_line_to (cr, EM_POINT(-6.0, 5.66));
	cairo_line_to (cr, EM_POINT(-4.1, 4.9));
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 0.7);
	cairo_set_line_width (cr, em);
	cairo_stroke_preserve (cr);
	cairo_fill (cr);

	cairo_pattern_destroy (pen);
#undef EM_POINT
}

/** Toolbar icon - Time Axis View reduce height */
static void icon_tav_shrink (cairo_t *cr, const int width, const int height)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y) * .66;
	const double ar = std::min (x, y) * .15;
	const double tri = .7 * (wh - ar);

	cairo_rectangle (cr, x - wh, y - ar, 2 * wh, 2 * ar);
	VECTORICONSTROKEFILL(.75);

	cairo_set_line_width (cr, 1.0);

	cairo_move_to (cr, x,       y - ar - 0.5);
	cairo_line_to (cr, x - tri, y - wh + 0.5);
	cairo_line_to (cr, x + tri, y - wh + 0.5);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .75);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);

	cairo_move_to (cr, x,       y + ar + 0.5);
	cairo_line_to (cr, x - tri, y + wh - 0.5);
	cairo_line_to (cr, x + tri, y + wh - 0.5);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .75);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);
}

/** Toolbar icon - Time Axis View increase height */
static void icon_tav_expand (cairo_t *cr, const int width, const int height)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y) * .66;
	const double ar = std::min (x, y) * .15;
	const double tri = .7 * (wh - ar);

	cairo_rectangle (cr, x - wh, y - wh, 2 * wh, 2 * wh);
	VECTORICONSTROKEFILL(.75);

	cairo_set_line_width (cr, 1.0);

	cairo_move_to (cr, x,       y - wh + 0.5);
	cairo_line_to (cr, x - tri, y - ar - 0.5);
	cairo_line_to (cr, x + tri, y - ar - 0.5);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .5);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);

	cairo_move_to (cr, x      , y + wh - 0.5);
	cairo_line_to (cr, x - tri, y + ar + 0.5);
	cairo_line_to (cr, x + tri, y + ar + 0.5);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .5);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);
}

/*****************************************************************************
 * Record enable (transport & track header).
 *
 * hardcoded "red" #f46f6f
 */

/** standard rec-enable circle */
static void icon_rec_enable (cairo_t *cr, const int width, const int height, const Gtkmm2ext::ActiveState state)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .55;
	cairo_arc (cr, x, y, r, 0, 2 * M_PI);
	if (state == Gtkmm2ext::ExplicitActive) {
		cairo_set_source_rgba (cr, 1.0, .1, .1, 1.0);
	}
	else if (state == Gtkmm2ext::ImplicitActive) {
		cairo_set_source_rgba (cr, .9, .3, .3, 1.0);
	}
	else {
		cairo_set_source_rgba (cr, .4, .3, .3, 1.0);
	}
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8); // outline
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/*****************************************************************************
 * Transport buttons, foreground is always white
 */

/** stop square box */
static void icon_transport_stop (cairo_t *cr, const int width, const int height)
{
	const int wh = std::min (width, height);
	cairo_rectangle (cr,
			(width - wh) * .5 + wh * .225,
			(height - wh) * .5 + wh * .225,
			wh * .55, wh * .55);
	VECTORICONSTROKEFILL(0.9); // small 'shine'
}

/** play triangle */
static void icon_transport_play (cairo_t *cr, const int width, const int height)
{
	const int wh = std::min (width, height) * .5;
	const double y = height * .5;
	const double x = width * .5;

	const double tri = ceil (.577 * wh); // 1/sqrt(3)

	cairo_move_to (cr,  x + wh * .5, y);
	cairo_line_to (cr,  x - wh * .5, y - tri);
	cairo_line_to (cr,  x - wh * .5, y + tri);
	cairo_close_path (cr);

	VECTORICONSTROKEFILL(0.9);
}

/** Midi Panic "!" */
static void icon_transport_panic (cairo_t *cr, const int width, const int height)
{
	const int wh = ceil (std::min (width, height) * .1) - .5;
	const double xc = rint (width * .5);
	const double yh = std::min (width, height);
	const double y0 = (height - yh) * .5;
	cairo_rectangle (cr,
	                 xc - wh, y0 + yh *.12,
	                 wh * 2,  yh *.48);
	VECTORICONSTROKEFILL(0.9);

	cairo_arc (cr, xc, y0 + yh *.78, wh, 0, 2 * M_PI);
	VECTORICONSTROKEFILL(0.9);
}

/** various combinations of lines and triangles "|>|", ">|" "|>" */
static void icon_transport_ck (cairo_t *cr,
		const enum ArdourWidgets::ArdourIcon::Icon icon,
		const int width, const int height)
{
	// small play triangle
	int wh = std::min (width, height);
	const double y = height * .5;
	const double x = width * .5;
	wh *= .18;
	const double tri = ceil (.577 * wh * 2); // 1/sqrt(3)

	const float ln = std::min (width, height) * .07;

	if (icon == TransportStart || icon == TransportRange) {
		cairo_rectangle (cr,
				x - wh - ln, y  - tri * 1.7,
				ln * 2,  tri * 3.4);

		VECTORICONSTROKEFILL(1.0);
	}

	if (icon == TransportEnd || icon == TransportRange) {
		cairo_rectangle (cr,
				x + wh - ln, y  - tri * 1.7,
				ln * 2,  tri * 3.4);

		VECTORICONSTROKEFILL(1.0);
	}

	if (icon == TransportStart) {
		cairo_move_to (cr,  x - wh, y);
		cairo_line_to (cr,  x + wh, y - tri);
		cairo_line_to (cr,  x + wh, y + tri);
	} else {
		cairo_move_to (cr,  x + wh, y);
		cairo_line_to (cr,  x - wh, y - tri);
		cairo_line_to (cr,  x - wh, y + tri);
	}

	cairo_close_path (cr);
	VECTORICONSTROKEFILL(1.0);
}

/** loop spiral */
static void icon_transport_loop (cairo_t *cr, const int width, const int height)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y);

	cairo_arc          (cr, x, y, r * .58, 0, 2 * M_PI);
	cairo_arc_negative (cr, x, y, r * .30, 2 * M_PI, 0);

	VECTORICONSTROKEFILL (1.0);

#define ARCARROW(rad, ang) \
	x + (rad) * sin ((ang) * 2.0 * M_PI), y + (rad) * cos ((ang) * 2.0 * M_PI)

	cairo_move_to (cr, ARCARROW(r * .30, .72));
	cairo_line_to (cr, ARCARROW(r * .11, .72));
	cairo_line_to (cr, ARCARROW(r * .55, .60));
	cairo_line_to (cr, ARCARROW(r * .74, .72));
	cairo_line_to (cr, ARCARROW(r * .58, .72));

	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_stroke_preserve (cr);
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
	cairo_fill (cr);
#undef ARCARROW
}

/** de-construct thorwil's metronom */
static void icon_transport_metronom (cairo_t *cr, const int width, const int height)
{
	const double x  = width * .5;
	const double y  = height * .5;
	const double wh = .95 * std::min (x, y);
	const double h  = wh * .80;
	const double w  = wh * .55;
	const double lw = w  * .34;

	cairo_rectangle (cr,
			x - w * .7, y + h * .25,
			w * 1.4, lw);

	VECTORICONSTROKEFILL(1.0);

	cairo_move_to (cr,  x - w,       y + h);
	cairo_line_to (cr,  x + w,       y + h);
	cairo_line_to (cr,  x + w * .35, y - h);
	cairo_line_to (cr,  x - w * .35, y - h);
	cairo_line_to (cr,  x - w,       y + h);

	cairo_move_to (cr,  x - w + lw,       y + h -lw);
	cairo_line_to (cr,  x - w * .35 + lw, y - h + lw);
	cairo_line_to (cr,  x + w * .35 - lw, y - h + lw);
	cairo_line_to (cr,  x + w - lw,       y + h -lw);
	cairo_line_to (cr,  x - w + lw,       y + h -lw);

	VECTORICONSTROKEFILL(1.0);

	// Pendulum
	// ddx = .70 w      = .75 * .5 wh              = .375 wh
	// ddy = .75 h - lw = .75 * .8 wh - wh .5 * .2 = .5 wh
	// ang = (ddx/ddy):
	// -> angle = atan (ang) = atan (375 / .5) ~= 36deg
	const double dx = lw * .2;  // 1 - cos(tan^-1(ang))
	const double dy = lw * .4;  // 1 - sin(tan^-1(ang))
	cairo_move_to (cr,  x - w * .3     , y + h * .25 + lw * .5);
	cairo_line_to (cr,  x - w + dx     , y - h + lw + dy);
	cairo_line_to (cr,  x - w + lw     , y - h + lw);
	cairo_line_to (cr,  x - w * .3 + lw, y + h * .25 + lw * .5);
	cairo_close_path (cr);

	VECTORICONSTROKEFILL(1.0);

	cairo_rectangle (cr,
			x - w * .7, y + h * .25,
			w * 1.4, lw);
	cairo_fill (cr);
}

/*****************************************************************************
 * Zoom: In "+", Out "-" and Full "[]"
 */
static void icon_zoom (cairo_t *cr, const enum ArdourWidgets::ArdourIcon::Icon icon, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .7;
	const double wh = std::min (x, y) * .45;

	// draw handle first
#define LINE45DEG(rad) \
	x + r * (rad) * .707, y + r * (rad) * .707 // sin(45deg) = cos(45deg) = .707
	cairo_move_to (cr, LINE45DEG(.9));
	cairo_line_to (cr, LINE45DEG(1.3));
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_width (cr, 3.0);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_stroke (cr);
#undef LINE45DEG

	// lens
	Gtkmm2ext::set_source_rgba (cr, fg_color);
	cairo_arc (cr, x, y, r, 0, 2 * M_PI);
	cairo_fill_preserve (cr);

	// add a lens gradient
	cairo_pattern_t *lens;
	lens = cairo_pattern_create_radial (x - r, y - r, r * .5, x - r, y - r, r * 2);
	cairo_pattern_add_color_stop_rgba (lens, 0, 1, 1, 1, .4);
	cairo_pattern_add_color_stop_rgba (lens, 1, 0, 0, 0, .4);
	cairo_set_source (cr, lens);
	cairo_fill_preserve (cr);
	cairo_pattern_destroy (lens);

	// outline
	cairo_set_line_width (cr, 1.5);
	//ardour_icon_set_source_inv_rgba (cr, fg_color); // alpha
	cairo_set_source_rgba (cr, .0, .0, .0, .8);
	cairo_stroke (cr);

	// add "+", "-" or "[]"
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_width (cr, .5 + DEFAULT_LINE_WIDTH);
	ardour_icon_set_source_inv_rgba (cr, fg_color);

	if (icon == ZoomIn || icon == ZoomOut) {
		cairo_move_to (cr, x - wh, y);
		cairo_line_to (cr, x + wh, y);
		cairo_stroke (cr);
	}
	if (icon == ZoomIn) {
		cairo_move_to (cr, x, y - wh);
		cairo_line_to (cr, x, y + wh);
		cairo_stroke (cr);
	}
	if (icon == ZoomFull) {
		const double br0 = std::min (x, y) * .1;
		const double br1 = std::min (x, y) * .3;
		const double bry = std::min (x, y) * .3;
		cairo_move_to (cr, x - br0, y - bry);
		cairo_line_to (cr, x - br1, y - bry);
		cairo_line_to (cr, x - br1, y + bry);
		cairo_line_to (cr, x - br0, y + bry);
		cairo_stroke (cr);

		cairo_move_to (cr, x + br0, y - bry);
		cairo_line_to (cr, x + br1, y - bry);
		cairo_line_to (cr, x + br1, y + bry);
		cairo_line_to (cr, x + br0, y + bry);
		cairo_stroke (cr);
	}
}

/** Toolbar icon - Mixbus Zoom Expand, rotated TimeAxisExpand */
static void icon_zoom_expand (cairo_t *cr, const int width, const int height)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y) * .66;
	const double ar = std::min (x, y) * .15;
	const double tri = .7 * (wh - ar);

	cairo_rectangle (cr, x - wh, y - wh, 2 * wh, 2 * wh);
	VECTORICONSTROKEFILL(.75);

	cairo_set_line_width (cr, 1.0);

	cairo_move_to (cr, x - wh + 0.5, y);
	cairo_line_to (cr, x - ar - 0.5, y - tri);
	cairo_line_to (cr, x - ar - 0.5, y + tri);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .5);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);

	cairo_move_to (cr, x + wh - 0.5, y);
	cairo_line_to (cr, x + ar + 0.5, y - tri);
	cairo_line_to (cr, x + ar + 0.5, y + tri);
	cairo_close_path (cr);

	cairo_set_source_rgba (cr, 1, 1, 1, .5);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill (cr);
}

/*****************************************************************************
 * Misc buttons
 */

/** "close" - "X" , no outline */
static void icon_close_cross (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double o = .5 + std::min (x, y) * .4;
	Gtkmm2ext::set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, DEFAULT_LINE_WIDTH);
	cairo_move_to (cr, x-o, y-o);
	cairo_line_to (cr, x+o, y+o);
	cairo_move_to (cr, x+o, y-o);
	cairo_line_to (cr, x-o, y+o);
	cairo_stroke (cr);
}

/** "hide" strike through eye */
static void icon_hide_eye (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y);

	const double r  = .2 * wh;
	const double o =  .60 * wh;
	const double dx = .75 * wh;
	const double dy = .65 * wh;

	cairo_move_to (cr, x - dx, y);
	cairo_curve_to (cr, x, y + dy, x, y + dy, x + dx, y);
	cairo_curve_to (cr, x, y - dy, x, y - dy, x - dx, y);
	VECTORICONSTROKE (DEFAULT_LINE_WIDTH, fg_color);

	cairo_arc (cr, x, y, r, 0, 2 * M_PI);
  //cairo_fill (cr);
	VECTORICONSTROKE (DEFAULT_LINE_WIDTH, fg_color);

	cairo_move_to (cr, x - o, y + o);
	cairo_line_to (cr, x + o, y - o);
	VECTORICONSTROKEOUTLINE (DEFAULT_LINE_WIDTH, fg_color);
}

/** slim "<" */
static void icon_scroll_left (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y);

	const double tri1 = .2 * wh;
	const double tri2 = .4 * wh;

	cairo_move_to (cr, x + tri1, y - tri2);
	cairo_line_to (cr, x - tri2, y);
	cairo_line_to (cr, x + tri1, y + tri2);
	VECTORICONSTROKE (DEFAULT_LINE_WIDTH, fg_color);
}

/** slim ">" */
static void icon_scroll_right (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{

	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y);

	const double tri1 = .2 * wh;
	const double tri2 = .4 * wh;

	cairo_move_to (cr, x - tri1, y - tri2);
	cairo_line_to (cr, x + tri2, y);
	cairo_line_to (cr, x - tri1, y + tri2);
	VECTORICONSTROKE (DEFAULT_LINE_WIDTH, fg_color);
}

/** "<" */
static void icon_nudge_left (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y);

	const double tri_x = .3 * wh;
	const double tri_y = .6 * wh;

	cairo_move_to (cr, x + tri_x, y - tri_y);
	cairo_line_to (cr, x - tri_x, y);
	cairo_line_to (cr, x + tri_x, y + tri_y);
	VECTORICONSTROKEOUTLINE(.5 + DEFAULT_LINE_WIDTH, fg_color);
}

/** ">" */
static void icon_nudge_right (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{

	const double x = width * .5;
	const double y = height * .5;
	const double wh = std::min (x, y);

	const double tri_x = .3 * wh;
	const double tri_y = .6 * wh;

	cairo_move_to (cr, x - tri_x, y - tri_y);
	cairo_line_to (cr, x + tri_x, y);
	cairo_line_to (cr, x - tri_x, y + tri_y);
	VECTORICONSTROKEOUTLINE(.5 + DEFAULT_LINE_WIDTH, fg_color);

}

/** mixer strip narrow/wide */
static void icon_strip_width (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double lw = DEFAULT_LINE_WIDTH;
	const double xm = rint (width * .5) - lw * .5;
	const double ym = rint (height * .5) - lw * .5;

	const double dx = ceil (width * .3);
	const double dy = ceil (height * .25);

	const double x0 = xm - dx;
	const double x1 = xm + dx;
	const double y0 = ym - dy;
	const double y1 = ym + dy;

	const double arx = width  * .15;
	const double ary = height * .15;

	Gtkmm2ext::set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, lw);

	// left + right
	cairo_move_to (cr, x0, y0);
	cairo_line_to (cr, x0, y1);
	cairo_move_to (cr, x1, y0);
	cairo_line_to (cr, x1, y1);

	// horiz center line
	cairo_move_to (cr, x0, ym);
	cairo_line_to (cr, x1, ym);

	// arrow left
	cairo_move_to (cr,  x0, ym);
	cairo_rel_line_to (cr, arx, -ary);
	cairo_move_to (cr,  x0, ym);
	cairo_rel_line_to (cr, arx, ary);

	// arrow right
	cairo_move_to (cr,  x1,  ym);
	cairo_rel_line_to (cr, -arx, -ary);
	cairo_move_to (cr,  x1,  ym);
	cairo_rel_line_to (cr, -arx, ary);
	cairo_stroke (cr);
}

/** 5-pin DIN MIDI socket */
static void icon_din_midi (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .75;
	Gtkmm2ext::set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, ceil (r * .05));
	cairo_arc (cr, x, y, r, .57 * M_PI, 2.43 * M_PI);
	cairo_stroke (cr);

	// pins equally spaced 45deg
	cairo_arc (cr, x, y * 0.5, r * .15, 0, 2 * M_PI);
	cairo_fill (cr);
	cairo_arc (cr, x * 0.5, y, r * .15, 0, 2 * M_PI);
	cairo_fill (cr);
	cairo_arc (cr, x * 1.5, y, r * .15, 0, 2 * M_PI);
	cairo_fill (cr);
	//  .5 + .5 * .5 * sin(45deg),  1.5 - .5 * .5 * cos(45deg)
	cairo_arc (cr, x * 0.677, y * .677, r * .15, 0, 2 * M_PI);
	cairo_fill (cr);
	cairo_arc (cr, x * 1.323, y * .677, r * .15, 0, 2 * M_PI);
	cairo_fill (cr);

	// bottom notch
	cairo_arc (cr, x, y+r, r * .26, 1.05 * M_PI, 1.95 * M_PI);
	cairo_stroke (cr);
}

/*****************************************************************************
 * Plugin Window Buttons
 */

static void icon_plus_sign (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double lw = DEFAULT_LINE_WIDTH;
	const double lc = fmod (lw * .5, 1.0);
	const double xc = rint (width * .5) - lc;
	const double yc = rint (height * .5) - lc;
	const double ln = rint (std::min (width, height) * .3);

	cairo_rectangle (cr, xc - lw * .5, yc - ln, lw,  ln * 2);
	cairo_rectangle (cr, xc - ln, yc - lw * .5, ln * 2,  lw);

  Gtkmm2ext::set_source_rgba (cr, fg_color);
  cairo_fill (cr);
}

static void icon_no_parking (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .6;
	const double rl = .7 * r;
	cairo_arc (cr, x, y, r, 0, 2. * M_PI);
	cairo_move_to (cr, x - rl, y - rl);
	cairo_line_to (cr, x + rl, y + rl);
	VECTORICONSTROKE (DEFAULT_LINE_WIDTH, fg_color);
}

static void icon_save_arrow_box (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;

	const double lw = DEFAULT_LINE_WIDTH;
	const double lc = fmod (lw * .5, 1.0);

	const double x0 = rint (x) - lc;
	const double y0 = rint (y + std::min (x, y) * .05) - lc;
	const double o0 = std::min (x, y) * .35;
	const double ww = rint (std::min (x, y) * .55);
	const double hh = rint (std::min (x, y) * .45);
	const double ar = .5 + std::min (x, y) * .1;

	/* box open at top middle */
	cairo_move_to (cr, x0 - o0, y0 - hh);
	cairo_line_to (cr, x0 - ww, y0 - hh);
	cairo_line_to (cr, x0 - ww, y0 + hh);
	cairo_line_to (cr, x0 + ww, y0 + hh);
	cairo_line_to (cr, x0 + ww, y0 - hh);
	cairo_line_to (cr, x0 + o0, y0 - hh);
	VECTORICONSTROKE (lw, fg_color);

	/* downward arrow into the box */
	cairo_move_to (cr, x0,      y0 - ar);
	cairo_line_to (cr, x0 - ar, y0 - ar);
	cairo_line_to (cr, x0,      y0);
	cairo_line_to (cr, x0 + ar, y0 - ar);
	cairo_line_to (cr, x0,      y0 - ar);
	cairo_line_to (cr, x0,      y0 - ww - ar);
	VECTORICONSTROKE (lw, fg_color);
}

static void icon_list_browse (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double d = std::min (x, y) * .5;
	const double r = std::min (x, y) * .1;
	const double l = std::min (x, y) * .2;
	const double lw = DEFAULT_LINE_WIDTH;

  Gtkmm2ext::set_source_rgba (cr, fg_color);
	cairo_arc (cr, x-d, y-d, r, 0, 2. * M_PI);
	cairo_fill (cr);
	cairo_arc (cr, x-d, y, r, 0, 2. * M_PI);
	cairo_fill (cr);
	cairo_arc (cr, x-d, y+d, r, 0, 2. * M_PI);
	cairo_fill (cr);

	cairo_move_to (cr, x - l, rint (y - d) + .5);
	cairo_line_to (cr, x + d, rint (y - d) + .5);
	cairo_move_to (cr, x - l, rint (y)     + .5);
	cairo_line_to (cr, x + d, rint (y)     + .5);
	cairo_move_to (cr, x - l, rint (y + d) + .5);
	cairo_line_to (cr, x + d, rint (y + d) + .5);
	VECTORICONSTROKE(lw, fg_color);
}

static void icon_on_off (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .65;
	const double lw = DEFAULT_LINE_WIDTH;
	const double lc = fmod (lw * .5, 1.0);
	const double x0 = rint (x) - lc;

	cairo_arc (cr, x0, y, r, -.3 * M_PI, 1.3 * M_PI);
	cairo_move_to (cr, x0, y - r);
	cairo_line_to (cr, x0, y);
	VECTORICONSTROKE (lw, fg_color);
}

static void icon_bypass (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double y0 = height * .6;
	const double r = std::min (x, y) * .75;
	const double o = std::min (x, y) * .275;
	const double pt = DEFAULT_LINE_WIDTH;

	const double dashes[] = { 1, pt };
	cairo_set_dash (cr, dashes, 2, 0);
	cairo_move_to (cr, x - r, y0);
	cairo_line_to (cr, x + r, y0);
	VECTORICONSTROKE(pt * .8, fg_color);
	cairo_set_dash (cr, 0, 0, 0);

	cairo_move_to (cr, x - o, y0 - o);
	cairo_line_to (cr, x + o, y0 + o);
	cairo_move_to (cr, x + o, y0 - o);
	cairo_line_to (cr, x - o, y0 + o);
	VECTORICONSTROKE(pt * .8, fg_color);

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_arc (cr, x, y0, r, 0, 0);
	cairo_arc (cr, x, y0, r *.8, 1.92 * M_PI, 1.92 * M_PI);
	cairo_arc (cr, x, y0, r * 1.17, 1.92 * M_PI, 1.92 * M_PI);
	cairo_close_path (cr);
	cairo_arc_negative (cr, x, y0, r, 0, M_PI);
	VECTORICONSTROKE(pt, fg_color);
}

static void icon_reset_knob (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r0 = std::min (x, y) * .3;
	const double r1 = std::min (x, y) * .65;
	const double ar = std::min (x, y) * .25;
	const double lw = DEFAULT_LINE_WIDTH;
	const double lc = fmod (lw * .5, 1.0);
	const double x0 = rint (x) - lc;

	cairo_arc (cr, x0, y, r0, 0, 2. * M_PI);
	cairo_move_to (cr, x0, y - r0);
	cairo_line_to (cr, x0, y);
	VECTORICONSTROKE(lw, fg_color);

	/* outer ring w/CCW arrow */
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_arc (cr, x0, y, r1, -.25 * M_PI, -.25 * M_PI);
	cairo_rel_line_to (cr, 0, ar);
	cairo_rel_line_to (cr, ar, -ar);
	cairo_arc (cr, x0, y, r1, -.25 * M_PI, -.25 * M_PI);
	cairo_arc (cr, x0, y, r1, -.25 * M_PI, 1.50 * M_PI);
	VECTORICONSTROKE(lw, fg_color);
}

static void icon_config_wheel (cairo_t *cr, const int width, const int height, const uint32_t fg_color, int arrow)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r0 = std::min (x, y) * .3;
	const double r1 = std::min (x, y) * .55;
	const double r2 = std::min (x, y) * .70;
	const double ar = std::min (x, y) * .25;
	const double lw = DEFAULT_LINE_WIDTH;

	for (int i = 0; i < 8; ++i) {
		double ang0 = i * 2.0 * M_PI / 8.0;
		double ang1 = (i + 1) * 2.0 * M_PI / 8.0;
		double angm = 2.0 * M_PI / 48.0;
		double angd = 2.0 * M_PI / 64.0;

		cairo_arc (cr, x, y, r2, ang0 - angm, ang0 + angm);
		cairo_arc (cr, x, y, r1, ang0 + angm + angd, ang1 - angm - angd);
	}
	cairo_close_path (cr);
	VECTORICONSTROKE(lw, fg_color);

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	if (arrow == 0) {
		cairo_arc (cr, x, y, r0, 0, 2.0 * M_PI);
	} else if (arrow > 0) {
		/* clockwise pointing arrow */
		cairo_arc (cr, x, y, r0, 1.9 * M_PI, 1.9 * M_PI);
		cairo_rel_line_to (cr, 0, -ar);
		cairo_rel_line_to (cr, -ar, ar);
		cairo_arc (cr, x, y, r0, 1.9 * M_PI, 1.9 * M_PI);
		cairo_arc_negative (cr, x, y, r0, 1.9 * M_PI, .5 * M_PI);
	} else {
		/* counterclockwise arrow */
		cairo_arc (cr, x, y, r0, 1.1 * M_PI, 1.1 * M_PI);
		cairo_rel_line_to (cr, 0, -ar);
		cairo_rel_line_to (cr, ar, ar);
		cairo_arc (cr, x, y, r0, 1.1 * M_PI, 1.1 * M_PI);
		cairo_arc (cr, x, y, r0, 1.1 * M_PI, .5 * M_PI);
	}
	VECTORICONSTROKE(lw, fg_color);
}

static void icon_pcb_via (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = ceil (width * .5) - .5;
	const double y = ceil (height * .5) - .5;

	const double d = rint (std::min (x, y) * .5);
	const double r = std::min (x, y) * .16;
	const double p = std::min (x, y) * .1;

	cairo_arc_negative (cr, x+d, y+d, r,        1.15 * M_PI, -.85 * M_PI);
	cairo_arc          (cr, x+d, y+d, d * 1.12, 1.15 * M_PI, 1.15 * M_PI);

	cairo_arc          (cr, x-d, y-d, d * 1.12, 0.15 * M_PI, .15 * M_PI);
	cairo_arc          (cr, x-d, y-d, r,        0.15 * M_PI, 2.5 * M_PI);

	cairo_arc          (cr, x-d, y-d, r,          .5 * M_PI,  .5 * M_PI);
	cairo_arc          (cr, x-d, y+d, r,         -.5 * M_PI, 1.5 * M_PI);
	VECTORICONSTROKE (p, fg_color);

	cairo_arc (cr, x+d, y-d, r, -.5 * M_PI, 1.5 * M_PI);
	VECTORICONSTROKE (p, fg_color);
}

static void icon_latency_clock (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double y0 = std::min (x, y) * .4;
	const double r0 = std::min (x, y) * .1;
	const double r1 = std::min (x, y) * .5;
	const double r2 = std::min (x, y) * .66;

	const double lw = DEFAULT_LINE_WIDTH;
	const double lc = fmod (lw * .5, 1.0);
	const double x0 = rint (x) - lc;

	cairo_move_to (cr, x0, y - y0);
	cairo_arc     (cr, x0, y, r2, -.5 * M_PI, 1.25 * M_PI);
	VECTORICONSTROKE(lw, fg_color);

	cairo_arc (cr, x0, y, r0,  -.4 * M_PI  , .9 * M_PI);
	cairo_arc (cr, x0, y, r1, 1.25 * M_PI, 1.25 * M_PI);
	cairo_arc (cr, x0, y, r0,  -.4 * M_PI,  -.4 * M_PI);
	cairo_close_path (cr);
  cairo_fill (cr);
}

/*****************************************************************************/

bool
ArdourWidgets::ArdourIcon::render (cairo_t *cr,
                                   const enum ArdourWidgets::ArdourIcon::Icon icon,
                                   const int width, const int height,
                                   const Gtkmm2ext::ActiveState state,
                                   const uint32_t fg_color)
{
	bool rv = true;
	cairo_save (cr);

	if (width < 6 || height < 6) {
		return false;
	}

	switch (icon) {
		case TransportStop:
			icon_transport_stop (cr, width, height);
			break;
		case TransportPlay:
			icon_transport_play (cr, width, height);
			break;
		case TransportLoop:
			icon_transport_loop (cr, width, height);
			break;
		case TransportMetronom:
			icon_transport_metronom (cr, width, height);
			break;
		case TransportPanic:
			icon_transport_panic (cr, width, height);
			break;
		case TransportStart:
			/* fallthrough */
		case TransportEnd:
			/* fallthrough */
		case TransportRange:
			icon_transport_ck (cr, icon, width, height);
			break;
		case RecButton:
			icon_rec_enable (cr, width, height, state);
			break;
		case CloseCross:
			icon_close_cross (cr, width, height, fg_color);
			break;
		case HideEye:
			icon_hide_eye (cr, width, height, fg_color);
			break;
		case StripWidth:
			icon_strip_width (cr, width, height, fg_color);
			break;
		case DinMidi:
			icon_din_midi (cr, width, height, fg_color);
			break;
		case ScrollLeft:
			icon_scroll_left (cr, width, height, fg_color);
			break;
		case ScrollRight:
			icon_scroll_right (cr, width, height, fg_color);
			break;
		case NudgeLeft:
			icon_nudge_left (cr, width, height, fg_color);
			break;
		case NudgeRight:
			icon_nudge_right (cr, width, height, fg_color);
			break;
		case ZoomIn:
			/* fallthrough */
		case ZoomOut:
			/* fallthrough */
		case ZoomFull:
			icon_zoom (cr, icon, width, height, fg_color);
			break;
		case ZoomExpand:
			icon_zoom_expand (cr, width, height);
			break;
		case TimeAxisShrink:
			icon_tav_shrink (cr, width, height);
			break;
		case TimeAxisExpand:
			icon_tav_expand (cr, width, height);
			break;
		case ToolRange:
			/* similar to icon_strip_width() but with outline */
			icon_tool_range (cr, width, height);
			break;
		case ToolGrab:
			icon_tool_grab (cr, width, height);
			break;
		case ToolCut:
			icon_tool_cut (cr, width, height);
			break;
		case ToolStretch:
			icon_tool_stretch (cr, width, height);
			break;
		case ToolAudition:
			icon_tool_audition (cr, width, height);
			break;
		case ToolDraw:
			icon_tool_draw (cr, width, height);
			break;
		case ToolContent:
			icon_tool_content (cr, width, height);
			break;
		case PsetAdd:
			icon_plus_sign (cr, width, height, fg_color);
			break;
		case PsetSave:
			icon_save_arrow_box (cr, width, height, fg_color);
			break;
		case PsetDelete:
			icon_no_parking (cr, width, height, fg_color);
			break;
		case PsetBrowse:
			icon_list_browse (cr, width, height, fg_color);
			break;
		case PluginReset:
			icon_reset_knob (cr, width, height, fg_color);
			break;
		case PluginBypass:
			icon_bypass (cr, width, height, fg_color);
			break;
		case PluginPinout:
			icon_pcb_via (cr, width, height, fg_color);
			break;
		case Config: /* unused */
			icon_config_wheel (cr, width, height, fg_color, 0);
			break;
		case ConfigReset: /* unused */
			icon_config_wheel (cr, width, height, fg_color, -1);
			break;
		case PowerOnOff: /* unused */
			icon_on_off (cr, width, height, fg_color);
			break;
		case LatencyClock: /* unused */
			icon_latency_clock (cr, width, height, fg_color);
			break;
		case NoIcon:
			rv = false;
			break;
	}
	cairo_restore (cr);
	return rv;
}
