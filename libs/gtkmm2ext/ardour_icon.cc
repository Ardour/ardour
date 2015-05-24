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

#include <math.h> // For M_PI
#include <algorithm>
#include "gtkmm2ext/ardour_icon.h"

// "canvas/types.h" Color == uint32_t
// from libs/canvas/utils.cc
static void ardour_canvas_set_source_rgba (cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba ( cr,
			((color >> 24) & 0xff) / 255.0,
			((color >> 16) & 0xff) / 255.0,
			((color >>  8) & 0xff) / 255.0,
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

#define VECTORICONSTROKEOUTLINE() \
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND); \
	cairo_set_line_width(cr, 3.0); \
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0); \
	cairo_stroke_preserve(cr); \
	ardour_canvas_set_source_rgba (cr, fg_color); \
	cairo_set_line_width(cr, 1.5);  \
	cairo_stroke(cr);


	/* TODO separate these into dedicated class
	 * it may also be efficient to render them only once for every size (image-surface) */
	switch (icon) {

	case Gtkmm2ext::ArdourIcon::RecTapeMode:
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
	case Gtkmm2ext::ArdourIcon::RecButton:
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
	case Gtkmm2ext::ArdourIcon::CloseCross:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double o = .5 + std::min(x, y) * .4;
		ardour_canvas_set_source_rgba (cr, fg_color);
		cairo_set_line_width(cr, 1);
		cairo_move_to(cr, x-o, y-o);
		cairo_line_to(cr, x+o, y+o);
		cairo_move_to(cr, x+o, y-o);
		cairo_line_to(cr, x-o, y+o);
		cairo_stroke(cr);
	}
	break;
	case Gtkmm2ext::ArdourIcon::StripWidth:
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

		ardour_canvas_set_source_rgba (cr, fg_color);
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
	case Gtkmm2ext::ArdourIcon::DinMidi:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double r = std::min(x, y) * .75;
		ardour_canvas_set_source_rgba (cr, fg_color);
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
	case Gtkmm2ext::ArdourIcon::TransportStop:
	{
		const int wh = std::min (width, height);
		cairo_rectangle (cr,
				(width - wh) * .5 + wh * .25,
				(height - wh) * .5 + wh * .25,
				wh * .5, wh * .5);

		VECTORICONSTROKEFILL(0.8);
	}
	break;
	case Gtkmm2ext::ArdourIcon::TransportPlay:
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
	case Gtkmm2ext::ArdourIcon::TransportPanic:
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
	case Gtkmm2ext::ArdourIcon::TransportStart:
	case Gtkmm2ext::ArdourIcon::TransportEnd:
	case Gtkmm2ext::ArdourIcon::TransportRange:
	{
		// small play triangle
		int wh = std::min (width, height);
		const double y = height * .5;
		const double x = width - wh * .5;
		wh *= .18;
		const float tri = ceil(.577 * wh * 2); // 1/sqrt(3)

		const float ln = std::min (width, height) * .07;

		if (icon == Gtkmm2ext::ArdourIcon::TransportStart || icon == Gtkmm2ext::ArdourIcon::TransportRange) {
			cairo_rectangle (cr,
					x - wh - ln, y  - tri * 1.7,
					ln * 2,  tri * 3.4);

			VECTORICONSTROKEFILL(1.0);
		}

		if (icon == Gtkmm2ext::ArdourIcon::TransportEnd || icon == Gtkmm2ext::ArdourIcon::TransportRange) {
			cairo_rectangle (cr,
					x + wh - ln, y  - tri * 1.7,
					ln * 2,  tri * 3.4);

			VECTORICONSTROKEFILL(1.0);
		}

		if (icon == Gtkmm2ext::ArdourIcon::TransportStart) {
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
	case Gtkmm2ext::ArdourIcon::TransportLoop:
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
	case Gtkmm2ext::ArdourIcon::TransportMetronom:
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
	case Gtkmm2ext::ArdourIcon::NudgeLeft:
	{
		const double x = width * .5;
		const double y = height * .5;
		const double wh = std::min (x, y);

		const double tri_x = .3 * wh;
		const double tri_y = .6 * wh;

		cairo_move_to (cr, x + tri_x, y - tri_y);
		cairo_line_to (cr, x - tri_x, y);
		cairo_line_to (cr, x + tri_x, y + tri_y);
		VECTORICONSTROKEOUTLINE();
	}
	break;
	case Gtkmm2ext::ArdourIcon::NudgeRight:
	{

		const double x = width * .5;
		const double y = height * .5;
		const double wh = std::min (x, y);

		const double tri_x = .3 * wh;
		const double tri_y = .6 * wh;

		cairo_move_to (cr, x - tri_x, y - tri_y);
		cairo_line_to (cr, x + tri_x, y);
		cairo_line_to (cr, x - tri_x, y + tri_y);
		VECTORICONSTROKEOUTLINE();

	}
	break;
	default:
		return false;
	} // end case(icon)

#undef VECTORICONSTROKEFILL
#undef VECTORICONSTROKEOUTLINE
	return true;
}


