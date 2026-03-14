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

#include "canvas/line.h"

#include "cross_cursor.h"

using namespace ArdourCanvas;

CrossCursor::CrossCursor (Item* parent)
	: _line_width (1)
	, _width (2)
	, _height (2)
	, _vline1 (parent)
	, _vline2 (parent)
	, _hline1 (parent)
	, _hline2 (parent)
{
	_vline1.set_ignore_events (true);
	_vline2.set_ignore_events (true);
	_hline1.set_ignore_events (true);
	_hline2.set_ignore_events (true);
}

void
CrossCursor::set_extents (double w, double h)
{
	_width = w;
	_height = h;

	_vline1.set_y1 (_vline1.y0() + _height);
	_vline2.set_y1 (_vline2.y0() + _height);
	_hline1.set_x1 (_hline1.x0() + _width);
	_hline2.set_x1 (_hline2.x0() + _width);
}

void
CrossCursor::set_position (Duple const & pos)
{
	_vline1.set (Duple (pos.x, 0), Duple (pos.x, _height));
	_vline2.set (Duple (pos.x + _line_width, 0), Duple (pos.x + _line_width, _height));
	_hline1.set (Duple (0, pos.y), Duple (_width, pos.y));
	_hline2.set (Duple (0, pos.y + _line_width), Duple (_width, pos.y + _line_width));
}

void
CrossCursor::set_outline_color (Gtkmm2ext::Color col)
{
	_vline1.set_outline_color (col);
	_vline2.set_outline_color (col);
	_hline1.set_outline_color (col);
	_hline2.set_outline_color (col);
}

void
CrossCursor::set_line_width (double w)
{
	_line_width = w;
}
