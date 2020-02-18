/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <cairomm/context.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "canvas/item.h"
#include "canvas/outline.h"
#include "canvas/debug.h"

using namespace ArdourCanvas;

Outline::Outline (Item& self)
	: _self (self)
	, _outline_color (0x000000ff)
	, _outline_width (1.0)
	, _outline (true)
{
}

void
Outline::set_outline_color (Gtkmm2ext::Color color)
{
	if (color != _outline_color) {
		_self.begin_visual_change ();
		_outline_color = color;
		_self.end_visual_change ();
	}
}

void
Outline::set_outline_width (Distance width)
{
	if (width != _outline_width) {
		_self.begin_change ();
		_outline_width = width;
		_self._bounding_box_dirty = true;
		_self.end_change ();
	}
}

void
Outline::set_outline (bool outline)
{
	if (outline != _outline) {
		_self.begin_change ();
		_outline = outline;
		_self._bounding_box_dirty = true;
		_self.end_change ();
	}
}

void
Outline::setup_outline_context (Cairo::RefPtr<Cairo::Context> context) const
{
	Gtkmm2ext::set_source_rgba (context, _outline_color);
	context->set_line_width (_outline_width);
}

