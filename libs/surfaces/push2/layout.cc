/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas.h"
#include "layout.h"
#include "push2.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourCanvas;

Push2Layout::Push2Layout (Push2& p, Session& s, std::string const & name)
	: Container (p.canvas())
	, p2 (p)
	, session (s)
	, _name (name)
{
}

Push2Layout::~Push2Layout ()
{
}

void
Push2Layout::compute_bounding_box () const
{
	/* all layouts occupy at least the full screen, even if their combined
	 * child boxes do not.
	 */
	_bounding_box = Rect (0, 0, display_width(), display_height());
	_bounding_box_dirty = false;
}

int
Push2Layout::display_height() const
{
	return p2.canvas()->rows();
}

int
Push2Layout::display_width() const
{
	return p2.canvas()->cols();
}
