/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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
*/

#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <cairomm/context.h>
#include "canvas/utils.h"

using std::max;
using std::min;

void
ArdourCanvas::color_to_hsv (Color color, double& h, double& s, double& v)
{
	double r, g, b, a;
	double cmax;
	double cmin;
	double delta;
	
	color_to_rgba (color, r, g, b, a);
	
	if (r > g) {
		cmax = max (r, b);
	} else {
		cmax = max (g, b);
	}

	if (r < g) {
		cmin = min (r, b);
	} else {
		cmin = min (g, b);
	}

	v = cmax;

	delta = cmax - cmin;

	if (cmax == 0) {
		// r = g = b == 0 ... v is undefined, s = 0
		s = 0.0;  
		h = -1.0;
	}

	if (delta != 0.0) {	
		if (cmax == r) {
			h = fmod ((g - b)/delta, 6.0);
		} else if (cmax == g) {
			h = ((b - r)/delta) + 2;
		} else {
			h = ((r - g)/delta) + 4;
		}
		
		h *= 60.0;
	}

	if (delta == 0 || cmax == 0) {
		s = 0;
	} else {
		s = delta / cmax;
	}

}

ArdourCanvas::Color
ArdourCanvas::hsv_to_color (double h, double s, double v, double a)
{
	s = min (1.0, max (0.0, s));
	v = min (1.0, max (0.0, v));

	if (s == 0) {
		// achromatic (grey)
		return rgba_to_color (v, v, v, a);
	}

	h = min (360.0, max (0.0, h));

	double c = v * s;
        double x = c * (1.0 - fabs(fmod(h / 60.0, 2) - 1.0));
        double m = v - c;

        if (h >= 0.0 && h < 60.0) {
		return rgba_to_color (c + m, x + m, m, a);
        } else if (h >= 60.0 && h < 120.0) {
		return rgba_to_color (x + m, c + m, m, a);
        } else if (h >= 120.0 && h < 180.0) {
		return rgba_to_color (m, c + m, x + m, a);
        } else if (h >= 180.0 && h < 240.0) {
		return rgba_to_color (m, x + m, c + m, a);
        } else if (h >= 240.0 && h < 300.0) {
		return rgba_to_color (x + m, m, c + m, a);
        } else if (h >= 300.0 && h < 360.0) {
		return rgba_to_color (c + m, m, x + m, a);
        } 
	return rgba_to_color (m, m, m, a);
}

void
ArdourCanvas::color_to_rgba (Color color, double& r, double& g, double& b, double& a)
{
	r = ((color >> 24) & 0xff) / 255.0;
	g = ((color >> 16) & 0xff) / 255.0;
	b = ((color >>  8) & 0xff) / 255.0;
	a = ((color >>  0) & 0xff) / 255.0;
}

ArdourCanvas::Color
ArdourCanvas::rgba_to_color (double r, double g, double b, double a)
{
	/* clamp to [0 .. 1] range */

	r = min (1.0, max (0.0, r));
	g = min (1.0, max (0.0, g));
	b = min (1.0, max (0.0, b));
	a = min (1.0, max (0.0, a));

	/* convert to [0..255] range */

	unsigned int rc, gc, bc, ac;
	rc = rint (r * 255.0);
	gc = rint (g * 255.0);
	bc = rint (b * 255.0);
	ac = rint (a * 255.0);

	/* build-an-integer */

	return (rc << 24) | (gc << 16) | (bc << 8) | ac;
}

void
ArdourCanvas::set_source_rgba (Cairo::RefPtr<Cairo::Context> context, Color color)
{
	context->set_source_rgba (
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		((color >>  0) & 0xff) / 255.0
		);
}

