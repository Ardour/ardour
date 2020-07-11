/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>

#include "pbd/unwind.h"

#include "canvas/canvas.h"
#include "canvas/cbox.h"
#include "canvas/constrained_item.h"

using namespace ArdourCanvas;
using namespace kiwi;
using std::cerr;
using std::endl;

cBox::cBox (Canvas* c, Orientation o)
	: ConstraintPacker (c)
	, collapse_on_hide (false)
	, homogenous (true)
{
}

cBox::cBox (Item* i, Orientation o)
	: ConstraintPacker (i)
	, collapse_on_hide (false)
	, homogenous (true)
{
}

void
cBox::child_changed (bool bbox_changed)
{
}


