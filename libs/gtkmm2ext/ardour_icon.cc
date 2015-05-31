/*
    Copyright (C) 2009 Paul Davis
    Copyright (C) 2015 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <math.h> // M_PI
#include <assert.h>
#include <algorithm> // std:min
#include "gtkmm2ext/ardour_icon.h"

using namespace Gtkmm2ext::ArdourIcon;

/* general style info:
 *
 * - geometry: icons should be centered, spanning
 *   wh = std::min (width * .5, height *.5) * .55;
 *
 * - all shapes should have a contrasting outline
 *   (usually white foreground, black outline)
 */

#define OUTLINEWIDTH 1.5 // px

#define VECTORICONSTROKEFILL(fillalpha)                    \
	cairo_set_line_width (cr, OUTLINEWIDTH);           \
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);          \
	cairo_stroke_preserve (cr);                        \
	cairo_set_source_rgba (cr, 1, 1, 1, (fillalpha));  \
	cairo_fill (cr);

#define VECTORICONSTROKEOUTLINE(LW, color)                 \
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);     \
	cairo_set_line_width (cr, (LW) + OUTLINEWIDTH);    \
	ardour_icon_set_source_inv_rgba (cr, color);       \
	cairo_stroke_preserve (cr);                        \
	ardour_icon_set_source_rgba (cr, color);           \
	cairo_set_line_width (cr, (LW));                   \
	cairo_stroke (cr);


/** convert 32bit 'RRGGBBAA' to cairo doubles
 * from libs/canvas/utils.cc and  canvas/types.h: typedef uint32_t Color;
 */
static void ardour_icon_set_source_rgba (cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba (cr,
			((color >> 24) & 0xff) / 255.0,
			((color >> 16) & 0xff) / 255.0,
			((color >>  8) & 0xff) / 255.0,
			((color >>  0) & 0xff) / 255.0
			);
}

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

		ardour_icon_set_source_rgba (cr, 0xffffffff);
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
	const double lw = rint (wh / 6.0); // line width
	const double ar = wh * .6; // arrow

	const double bw = ceil (wh) - .5;
	const double y0 = ceil (y);
	const double ym = rint (y0 - wh * .1) + .5; // arrow-horizontal; slightly to the top, on a px
	const double x0 = rint (x) - bw; // left arrow tip
	const double x1 = rint (x) + bw; // right arrow tip

	// left and right box
	cairo_move_to (cr, x0, y0 - bw);
	cairo_line_to (cr, x0, y0 + bw);
	VECTORICONSTROKEOUTLINE(lw, 0xffffffff);
	cairo_move_to (cr, x1, y0 - bw);
	cairo_line_to (cr, x1, y0 + bw);
	VECTORICONSTROKEOUTLINE(lw, 0xffffffff);

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
	VECTORICONSTROKEOUTLINE(lw, 0xffffffff);

	cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
	cairo_set_line_width (cr, lw);

	cairo_move_to (cr, x0, y0 - bw);
	cairo_line_to (cr, x0, y0 + bw);
	cairo_stroke (cr);

	cairo_move_to (cr, x1, y0 - bw);
	cairo_line_to (cr, x1, y0 + bw);
	cairo_stroke (cr);


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
	if (state == Gtkmm2ext::ExplicitActive)
		cairo_set_source_rgba (cr, 0.95, 0.10, 0.10, 1.0);
	else
		cairo_set_source_rgba (cr, 0.95, 0.44, 0.44, 1.0); // #f46f6f
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8); // outline
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/** tape-mode, "reel" */
static void icon_rec_tape (cairo_t *cr, const int width, const int height, const Gtkmm2ext::ActiveState state)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .6;
	const double slit = .11 * M_PI;
	cairo_translate (cr, x, y);

	cairo_arc (cr, 0, 0, r, 0, 2 * M_PI);
	if (state == Gtkmm2ext::ExplicitActive) {
		cairo_set_source_rgba (cr, .95, .1, .1, 1.);
	} else {
		cairo_set_source_rgba (cr, .95, .44, .44, 1.); // #f46f6f
	}
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, .0, .0, .0, .5);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);

	cairo_save (cr);
	cairo_set_source_rgba (cr, .15, .07, .07, 1.0);

	cairo_rotate (cr, -.5 * M_PI);
	cairo_move_to (cr, 0, 0);
	cairo_arc (cr, 0, 0, r *.85, -slit, slit);
	cairo_line_to (cr, 0, 0);
	cairo_close_path (cr);

	cairo_fill (cr);
	cairo_rotate (cr, 2. * M_PI / 3.);

	cairo_move_to (cr, 0, 0);
	cairo_arc (cr, 0, 0, r *.85, -slit, slit);
	cairo_line_to (cr, 0, 0);
	cairo_close_path (cr);
	cairo_fill (cr);

	cairo_rotate (cr, 2. * M_PI / 3.);
	cairo_move_to (cr, 0, 0);
	cairo_arc (cr, 0, 0, r *.85, -slit, slit);
	cairo_line_to (cr, 0, 0);
	cairo_close_path (cr);
	cairo_fill (cr);

	cairo_restore (cr);

	cairo_arc (cr, 0, 0, r * .3, 0, 2 * M_PI);
	if (state == Gtkmm2ext::ExplicitActive)
		cairo_set_source_rgba (cr, .95, .1, .1, 1.);
	else
		cairo_set_source_rgba (cr, .95, .44, .44, 1.); // #f46f6f
	cairo_fill (cr);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_arc (cr, 0, 0, r *.15, 0, 2 * M_PI); // hole in the middle
	cairo_fill (cr);
}


/*****************************************************************************
 * Transport buttons, foreground is always white
 */

/** stop square box */
static void icon_transport_stop (cairo_t *cr, const int width, const int height)
{
	const int wh = std::min (width, height);
	cairo_rectangle (cr,
			(width - wh) * .5 + wh * .25,
			(height - wh) * .5 + wh * .25,
			wh * .5, wh * .5);
	VECTORICONSTROKEFILL(0.9); // small 'shine'
}

/** play triangle */
static void icon_transport_play (cairo_t *cr, const int width, const int height)
{
	const int wh = std::min (width, height) * .5;
	const double y = height * .5;
	const double x = width - wh;

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
	const int wh = std::min (width, height) * .1;
	const double xc = width * .5;
	const double yh = height;
	cairo_rectangle (cr,
	                 xc - wh, yh *.19,
	                 wh * 2,  yh *.41);
	VECTORICONSTROKEFILL(0.9);

	cairo_arc (cr, xc, yh *.75, wh, 0, 2 * M_PI);
	VECTORICONSTROKEFILL(0.9);
}

/** various combinations of lines and triangles "|>|", ">|" "|>" */
static void icon_transport_ck (cairo_t *cr,
		const enum Gtkmm2ext::ArdourIcon::Icon icon,
		const int width, const int height)
{
	// small play triangle
	int wh = std::min (width, height);
	const double y = height * .5;
	const double x = width - wh * .5;
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

	cairo_arc          (cr, x, y, r * .62, 0, 2 * M_PI);
	cairo_arc_negative (cr, x, y, r * .35, 2 * M_PI, 0);

	VECTORICONSTROKEFILL(1.0);

#define ARCARROW(rad, ang) \
	x + (rad) * sin ((ang) * 2.0 * M_PI), y + (rad) * cos ((ang) * 2.0 * M_PI)

	cairo_move_to (cr, ARCARROW(r * .35, .72));
	cairo_line_to (cr, ARCARROW(r * .15, .72));
	cairo_line_to (cr, ARCARROW(r * .56, .60));
	cairo_line_to (cr, ARCARROW(r * .75, .72));
	cairo_line_to (cr, ARCARROW(r * .62, .72));

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
	const double wh = std::min (x, y);
	const double h  = wh * .85;
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
static void icon_zoom (cairo_t *cr, const enum Gtkmm2ext::ArdourIcon::Icon icon, const int width, const int height, const uint32_t fg_color)
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
	ardour_icon_set_source_rgba (cr, fg_color);
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
	cairo_set_line_width (cr, 1.5);
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


/*****************************************************************************
 * Misc buttons
 */

/** "close" - "X" , no outline */
static void icon_close_cross (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double o = .5 + std::min (x, y) * .4;
	ardour_icon_set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, 1);
	cairo_move_to (cr, x-o, y-o);
	cairo_line_to (cr, x+o, y+o);
	cairo_move_to (cr, x+o, y-o);
	cairo_line_to (cr, x-o, y+o);
	cairo_stroke (cr);
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
	VECTORICONSTROKEOUTLINE(1.5, fg_color);
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
	VECTORICONSTROKEOUTLINE(1.5, fg_color);

}

/** mixer strip narrow/wide */
static void icon_strip_width (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x0 = width   * .2;
	const double x1 = width   * .8;

	const double y0 = height  * .25;
	const double y1= height   * .75;

	const double ym= height   * .5;

	// arrow
	const double xa0= height  * .39;
	const double xa1= height  * .61;
	const double ya0= height  * .35;
	const double ya1= height  * .65;

	ardour_icon_set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, 1);

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
	cairo_line_to (cr, xa0, ya0);
	cairo_move_to (cr,  x0, ym);
	cairo_line_to (cr, xa0, ya1);

	// arrow right
	cairo_move_to (cr,  x1,  ym);
	cairo_line_to (cr, xa1, ya0);
	cairo_move_to (cr,  x1,  ym);
	cairo_line_to (cr, xa1, ya1);
	cairo_stroke (cr);
}

/** 5-pin DIN MIDI socket */
static void icon_din_midi (cairo_t *cr, const int width, const int height, const uint32_t fg_color)
{
	const double x = width * .5;
	const double y = height * .5;
	const double r = std::min (x, y) * .75;
	ardour_icon_set_source_rgba (cr, fg_color);
	cairo_set_line_width (cr, 1);
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


/*****************************************************************************/

bool
Gtkmm2ext::ArdourIcon::render (cairo_t *cr,
                               const enum Gtkmm2ext::ArdourIcon::Icon icon,
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
		case TransportStart: // no break
		case TransportEnd:   // no break
		case TransportRange:
			icon_transport_ck (cr, icon, width, height);
			break;
		case RecTapeMode:
			icon_rec_tape (cr, width, height, state);
			break;
		case RecButton:
			icon_rec_enable (cr, width, height, state);
			break;
		case CloseCross:
			icon_close_cross (cr, width, height, fg_color);
			break;
		case StripWidth:
			icon_strip_width (cr, width, height, fg_color);
			break;
		case DinMidi:
			icon_din_midi (cr, width, height, fg_color);
			break;
		case NudgeLeft:
			icon_nudge_left (cr, width, height, fg_color);
			break;
		case NudgeRight:
			icon_nudge_right (cr, width, height, fg_color);
			break;
		case ZoomIn:  // no break
		case ZoomOut: // no break
		case ZoomFull:
			icon_zoom (cr, icon, width, height, fg_color);
			break;
		case TimeAxisShrink:
			icon_tav_shrink (cr, width, height);
			break;
		case TimeAxisExpand:
			icon_tav_expand (cr, width, height);
			break;
		case ToolRange:
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
		default:
			rv = false;
			break;
	}
	cairo_restore (cr);
	return rv;
}

#undef VECTORICONSTROKEFILL
#undef VECTORICONSTROKEOUTLINE
