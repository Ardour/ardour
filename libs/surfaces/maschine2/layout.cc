/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include "maschine2.h"
#include "canvas.h"
#include "layout.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourCanvas;

Maschine2Layout::Maschine2Layout (Maschine2& m2, Session& s, const std::string& name)
	: Container (m2.canvas())
	, _m2 (m2)
	, _session (s)
	, _name (name)
{
}

Maschine2Layout::~Maschine2Layout ()
{
}

void
Maschine2Layout::compute_bounding_box () const
{
	/* all layouts occupy at least the full screen, even if their combined
	 * child boxes do not.
	 */
	_bounding_box = Rect (0, 0, display_width(), display_height());
	_bounding_box_dirty = false;
}

int
Maschine2Layout::display_height() const
{
	return _m2.canvas()->height();
}

int
Maschine2Layout::display_width() const
{
	return _m2.canvas()->width();
}
