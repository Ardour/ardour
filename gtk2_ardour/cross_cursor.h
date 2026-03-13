/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include "canvas/types.h"
#include "gtkmm2ext/colors.h"

namespace ArdourCanvas {
class Canvas;
class Item;
class Line;
}

class CrossCursor
{
  public:
	CrossCursor (ArdourCanvas::Canvas*);
	CrossCursor (ArdourCanvas::Item*);

	void set_extents (double w, double h);

	ArdourCanvas::Duple position() const;
	void set_position (ArdourCanvas::Duple const &);
	void set_line_width (double);
	void set_fill_color (Gtkmm2ext::Color);
	void set_outline_color (Gtkmm2ext::Color);
	void set_fill (bool);
	void set_outline (bool);

  private:
	double _line_width;
	double _width;
	double _height;
	ArdourCanvas::Line _vline;
	ArdourCanvas::Line _hline;
};
