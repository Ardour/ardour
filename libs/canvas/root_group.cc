/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/stacktrace.h"

#include "canvas/root_group.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Root::Root (Canvas* canvas)
	: Container (canvas)
{
#ifdef CANVAS_DEBUG
	name = "ROOT";
#endif
}

void
Root::size_request (Distance& w, Distance& h) const
{
	if (_items.size() == 1) {
		_items.front()->size_request (w, h);
		return;
	}

	Item::size_request (w, h);
}

