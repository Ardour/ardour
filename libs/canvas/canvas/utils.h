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

#include "canvas/types.h"

namespace ArdourCanvas {

	extern void color_to_hsv (Color color, double& h, double& s, double& v);
	extern Color hsv_to_color (double h, double s, double v, double a);

	extern void color_to_rgba (Color, double& r, double& g, double& b, double& a);
	extern Color rgba_to_color (double r, double g, double b, double a);

	extern void set_source_rgba (Cairo::RefPtr<Cairo::Context>, Color);
}

