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
#include <algorithm> // std:min
#include "gtkmm2ext/ardour_icon.h"

using namespace Gtkmm2ext::ArdourIcon;

// from libs/canvas/utils.cc and  canvas/types.h: typedef uint32_t Color;
static void ardour_icon_set_source_rgba (cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba (cr,
			((color >> 24) & 0xff) / 255.0,
			((color >> 16) & 0xff) / 255.0,
			((color >>  8) & 0xff) / 255.0,
			((color >>  0) & 0xff) / 255.0
			);
}

static void ardour_icon_set_source_inv_rgba (cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba (cr,
			1.0 - ((color >> 24) & 0xff) / 255.0,
			1.0 - ((color >> 16) & 0xff) / 255.0,
			1.0 - ((color >>  8) & 0xff) / 255.0,
			((color >>  0) & 0xff) / 255.0
			);
}

bool
Gtkmm2ext::ArdourIcon::render (cairo_t *cr,
                               const enum Gtkmm2ext::ArdourIcon::Icon icon,
                               const int width, const int height,
                               const Gtkmm2ext::ActiveState state,
                               const uint32_t fg_color)
{

#define VECTORICONSTROKEFILL(fillalpha) \
	cairo_set_line_width(cr, 1.5); \
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0); \
	cairo_stroke_preserve(cr); \
	cairo_set_source_rgba (cr, 1, 1, 1, (fillalpha)); \
	cairo_fill(cr);

#define VECTORICONSTROKEOUTLINE(LW, color) \
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND); \
	cairo_set_line_width(cr, (LW) + 1.5); \
	ardour_icon_set_source_inv_rgba (cr, color); \
	cairo_stroke_preserve(cr); \
	ardour_icon_set_source_rgba (cr, color); \
	cairo_set_line_width(cr, (LW));  \
	cairo_stroke(cr);

	switch (icon) {

	case RecTapeMode:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double r = std::min(x, y) * .6;
		const double slit = .11 * M_PI;
		cairo_save(cr);
		cairo_translate(cr, x, y);

		cairo_arc (cr, 0, 0, r, 0, 2 * M_PI);
		if (state == Gtkmm2ext::ExplicitActive) {
			cairo_set_source_rgba (cr, .95, .1, .1, 1.);
		} else {
			cairo_set_source_rgba (cr, .95, .44, .44, 1.); // #f46f6f
		}
		cairo_fill_preserve(cr);
		cairo_set_source_rgba (cr, .0, .0, .0, .5);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);

		cairo_save(cr);
		cairo_set_source_rgba (cr, .15, .07, .07, 1.0);

		cairo_rotate (cr, -.5 * M_PI);
		cairo_move_to(cr, 0, 0);
		cairo_arc (cr, 0, 0, r *.85, -slit, slit);
		cairo_line_to(cr, 0, 0);
		cairo_close_path(cr);

		cairo_fill(cr);
		cairo_rotate (cr, 2. * M_PI / 3.);

		cairo_move_to(cr, 0, 0);
		cairo_arc (cr, 0, 0, r *.85, -slit, slit);
		cairo_line_to(cr, 0, 0);
		cairo_close_path(cr);
		cairo_fill(cr);

		cairo_rotate (cr, 2. * M_PI / 3.);
		cairo_move_to(cr, 0, 0);
		cairo_arc (cr, 0, 0, r *.85, -slit, slit);
		cairo_line_to(cr, 0, 0);
		cairo_close_path(cr);
		cairo_fill(cr);

		cairo_restore(cr);

		cairo_arc (cr, 0, 0, r * .3, 0, 2 * M_PI);
		if (state == Gtkmm2ext::ExplicitActive)
			cairo_set_source_rgba (cr, .95, .1, .1, 1.);
		else
			cairo_set_source_rgba (cr, .95, .44, .44, 1.); // #f46f6f
		cairo_fill(cr);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_arc (cr, 0, 0, r *.15, 0, 2 * M_PI); // hole in the middle
		cairo_fill(cr);

		cairo_restore(cr);
	}
	break;
	case RecButton:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double r = std::min(x, y) * .55;
		cairo_arc (cr, x, y, r, 0, 2 * M_PI);
		if (state == Gtkmm2ext::ExplicitActive)
			cairo_set_source_rgba (cr, .95, .1, .1, 1.);
		else
			cairo_set_source_rgba (cr, .95, .44, .44, 1.); // #f46f6f
		cairo_fill_preserve(cr);
		cairo_set_source_rgba (cr, .0, .0, .0, .8);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
	}
	break;
	case CloseCross:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double o = .5 + std::min(x, y) * .4;
		ardour_icon_set_source_rgba (cr, fg_color);
		cairo_set_line_width(cr, 1);
		cairo_move_to(cr, x-o, y-o);
		cairo_line_to(cr, x+o, y+o);
		cairo_move_to(cr, x+o, y-o);
		cairo_line_to(cr, x-o, y+o);
		cairo_stroke(cr);
	}
	break;
	case StripWidth:
	{
		const double x0 = width  * .2;
		const double x1 = width  * .8;

		const double y0 = height * .25;
		const double y1= height  * .75;

		const double ym= height  * .5;

		// arrow
		const double xa0= height  * .39;
		const double xa1= height  * .61;
		const double ya0= height  * .35;
		const double ya1= height  * .65;

		ardour_icon_set_source_rgba (cr, fg_color);
		cairo_set_line_width(cr, 1);

		// left + right
		cairo_move_to(cr, x0, y0);
		cairo_line_to(cr, x0, y1);
		cairo_move_to(cr, x1, y0);
		cairo_line_to(cr, x1, y1);

		// horiz center line
		cairo_move_to(cr, x0, ym);
		cairo_line_to(cr, x1, ym);

		// arrow left
		cairo_move_to(cr,  x0, ym);
		cairo_line_to(cr, xa0, ya0);
		cairo_move_to(cr,  x0, ym);
		cairo_line_to(cr, xa0, ya1);

		// arrow right
		cairo_move_to(cr,  x1,  ym);
		cairo_line_to(cr, xa1, ya0);
		cairo_move_to(cr,  x1,  ym);
		cairo_line_to(cr, xa1, ya1);
		cairo_stroke(cr);
	}
	break;
	case DinMidi:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double r = std::min(x, y) * .75;
		ardour_icon_set_source_rgba (cr, fg_color);
		cairo_set_line_width(cr, 1);
		cairo_arc (cr, x, y, r, .57 * M_PI, 2.43 * M_PI);
		cairo_stroke(cr);

		// pins equally spaced 45deg
		cairo_arc (cr, x, y * 0.5, r * .15, 0, 2 * M_PI);
		cairo_fill(cr);
		cairo_arc (cr, x * 0.5, y, r * .15, 0, 2 * M_PI);
		cairo_fill(cr);
		cairo_arc (cr, x * 1.5, y, r * .15, 0, 2 * M_PI);
		cairo_fill(cr);
		//  .5 + .5 * .5 * sin(45deg),  1.5 - .5 * .5 * cos(45deg)
		cairo_arc (cr, x * 0.677, y * .677, r * .15, 0, 2 * M_PI);
		cairo_fill(cr);
		cairo_arc (cr, x * 1.323, y * .677, r * .15, 0, 2 * M_PI);
		cairo_fill(cr);

		// bottom notch
		cairo_arc (cr, x, y+r, r * .26, 1.05 * M_PI, 1.95 * M_PI);
		cairo_stroke(cr);
	}
	break;
	case TransportStop:
	{
		const int wh = std::min (width, height);
		cairo_rectangle (cr,
				(width - wh) * .5 + wh * .25,
				(height - wh) * .5 + wh * .25,
				wh * .5, wh * .5);

		VECTORICONSTROKEFILL(0.8);
	}
	break;
	case TransportPlay:
	{
		const int wh = std::min (width, height) * .5;
		const double y = height * .5;
		const double x = width - wh;

		const float tri = ceil(.577 * wh); // 1/sqrt(3)

		cairo_move_to (cr,  x + wh * .5, y);
		cairo_line_to (cr,  x - wh * .5, y - tri);
		cairo_line_to (cr,  x - wh * .5, y + tri);
		cairo_close_path (cr);

		VECTORICONSTROKEFILL(0.8);
	}
	break;
	case TransportPanic:
	{
		const int wh = std::min (width, height) * .1;
		const double xc = width * .5;
		const double yh = height;
		cairo_rectangle (cr,
				xc - wh, yh *.19,
				wh * 2,  yh *.41);
		VECTORICONSTROKEFILL(0.8);

		cairo_arc (cr, xc, yh *.75, wh, 0, 2 * M_PI);
		VECTORICONSTROKEFILL(0.8);
	}
	break;
	case TransportStart:
	case TransportEnd:
	case TransportRange:
	{
		// small play triangle
		int wh = std::min (width, height);
		const double y = height * .5;
		const double x = width - wh * .5;
		wh *= .18;
		const float tri = ceil(.577 * wh * 2); // 1/sqrt(3)

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
	break;
	case TransportLoop:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double r = std::min(x, y);

		cairo_arc          (cr, x, y, r * .62, 0, 2 * M_PI);
		cairo_arc_negative (cr, x, y, r * .35, 2 * M_PI, 0);

		VECTORICONSTROKEFILL(1.0);
#define ARCARROW(rad, ang) \
		x + (rad) * sin((ang) * 2.0 * M_PI), y + (rad) * cos((ang) * 2.0 * M_PI)

		cairo_move_to (cr, ARCARROW(r * .35, .72));
		cairo_line_to (cr, ARCARROW(r * .15, .72));
		cairo_line_to (cr, ARCARROW(r * .56, .60));
		cairo_line_to (cr, ARCARROW(r * .75, .72));
		cairo_line_to (cr, ARCARROW(r * .62, .72));

		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_stroke_preserve(cr);
		cairo_close_path (cr);
		cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
		cairo_fill(cr);
#undef ARCARROW
	}
	break;
	case TransportMetronom:
	{
		const double x  = width * .5;
		const double y  = height * .5;
		const double wh = std::min(x, y);
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
		cairo_fill(cr);
	}
	break;
	case NudgeLeft:
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
	break;
	case NudgeRight:
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
	break;
	case ZoomIn:
	case ZoomOut:
	case ZoomFull:
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
	break;
	case TimeAxisShrink:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double wh = std::min (x, y) * .66;
		const double ar = std::min (x, y) * .15;
		const double tri = .7 * (wh - ar);

		cairo_rectangle (cr, x - wh, y - ar, 2 * wh, 2 * ar);
		VECTORICONSTROKEFILL(.75);

		cairo_set_line_width(cr, 1.0);

		cairo_move_to (cr, x,       y - ar - 0.5);
		cairo_line_to (cr, x - tri, y - wh + 0.5);
		cairo_line_to (cr, x + tri, y - wh + 0.5);
		cairo_close_path (cr);

		cairo_set_source_rgba (cr, 1, 1, 1, .75);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_fill(cr);

		cairo_move_to (cr, x,       y + ar + 0.5);
		cairo_line_to (cr, x - tri, y + wh - 0.5);
		cairo_line_to (cr, x + tri, y + wh - 0.5);
		cairo_close_path (cr);

		cairo_set_source_rgba (cr, 1, 1, 1, .75);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_fill(cr);
	}
	break;
	case TimeAxisExpand:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double wh = std::min (x, y) * .66;
		const double ar = std::min (x, y) * .15;
		const double tri = .7 * (wh - ar);

		cairo_rectangle (cr, x - wh, y - wh, 2 * wh, 2 * wh);
		VECTORICONSTROKEFILL(.75);

		cairo_set_line_width(cr, 1.0);

		cairo_move_to (cr, x,       y - wh + 0.5);
		cairo_line_to (cr, x - tri, y - ar - 0.5);
		cairo_line_to (cr, x + tri, y - ar - 0.5);
		cairo_close_path (cr);

		cairo_set_source_rgba (cr, 1, 1, 1, .5);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_fill(cr);

		cairo_move_to (cr, x      , y + wh - 0.5);
		cairo_line_to (cr, x - tri, y + ar + 0.5);
		cairo_line_to (cr, x + tri, y + ar + 0.5);
		cairo_close_path (cr);

		cairo_set_source_rgba (cr, 1, 1, 1, .5);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_fill(cr);
	}
	break;
	case ToolRange:
	{
		const double x  = width * .5;
		const double y  = height * .5;
		const double wh = std::min (x, y) * .6;
		const double lw = wh / 6.0; // line width (1px with 20x20 button)
		const double ar = wh * .5; // arrow
		const double ym = rint (y - wh * .1) + .5; // slightly to the top, on a px

		const double x0 = x - wh;
		const double x1 = x + wh;

		cairo_rectangle (cr,
				x - wh - lw,
				y - wh,
				lw,  2 * wh);

		VECTORICONSTROKEFILL(1.0);

		cairo_rectangle (cr,
				x + wh,
				y - wh,
				lw,  2 * wh);

		VECTORICONSTROKEFILL(1.0);

		cairo_save (cr);

		// don't draw outline inside the boxes
		cairo_rectangle (cr, x0, y - wh,
				2 * wh, 2 * wh);
		cairo_clip (cr);

		// arrow
		cairo_move_to (cr, x0 + ar, ym - ar);
		cairo_line_to (cr, x0, ym);
		cairo_line_to (cr, x0 + ar, ym + ar);

		cairo_move_to (cr, x1 - ar, ym - ar);
		cairo_line_to (cr, x1, ym);
		cairo_line_to (cr, x1 - ar, ym + ar);

		cairo_move_to (cr, x0, ym);
		cairo_line_to (cr, x1, ym);
		VECTORICONSTROKEOUTLINE(lw, 0xffffffff);

		cairo_restore (cr);
	}
	break;
	case ToolGrab:
	{
		const double x  = width * .5;
		const double y  = height * .5;
		const double em = std::min (x, y) * .15; // 3px at 20x20

		// 6x8em hand, with 'em' wide index finger.
#define EM_POINT(X,Y) \
		x + (X) * em, y + (Y) * em

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
		cairo_line_to (cr, EM_POINT(-0.8,  0.0));
		cairo_line_to (cr, EM_POINT( 0.3, -0.4));
		cairo_line_to (cr, EM_POINT( 0.4, -0.6));
		cairo_line_to (cr, EM_POINT( 0.5, -0.4));
		cairo_line_to (cr, EM_POINT( 0.5,  0.1));

		// ring finger knuckle
		cairo_line_to (cr, EM_POINT( 1.0,  0.2));
		cairo_line_to (cr, EM_POINT( 1.4, -0.3));
		cairo_line_to (cr, EM_POINT( 1.5, -0.5));
		cairo_line_to (cr, EM_POINT( 1.6, -0.3));
		cairo_line_to (cr, EM_POINT( 1.6,  0.3));

		// pinky
		cairo_line_to (cr, EM_POINT( 2.0,  0.5));
		cairo_line_to (cr, EM_POINT( 2.5,  0.1));
		cairo_line_to (cr, EM_POINT( 2.6,  0.0));
		cairo_line_to (cr, EM_POINT( 2.7,  0.1));
		cairo_line_to (cr, EM_POINT( 3.0,  1.0));

		// wrist
		cairo_line_to (cr, EM_POINT( 3.0,  1.5));
		cairo_line_to (cr, EM_POINT( 2.0,  4.0));

		cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
		VECTORICONSTROKEFILL(1.0);
	}
	break;
	default:
		return false;
	} // end switch (icon)

#undef VECTORICONSTROKEFILL
#undef VECTORICONSTROKEOUTLINE
	return true;
}
