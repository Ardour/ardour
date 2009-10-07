/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#include "diamond.h"

using namespace Gnome::Canvas;
using namespace Gnome::Art;

Diamond::Diamond(Group& group, double height)
	: Polygon(group)
	, _x (0)
	, _y (0)
	, _h (height)
{
	points = gnome_canvas_points_new (4);
	g_object_set (gobj(), "points", points, NULL);
	move_to (0, 0);
}

Diamond::~Diamond ()
{
	gnome_canvas_points_free (points);
}

void
Diamond::set_height (double height)
{
	_h = height;
	move_to (_x, _y);
}

void
Diamond::move_to (double x, double y)
{
	_x = x;
	_y = y;

	points->coords[0] = _x;
	points->coords[1] = _y + (_h * 2.0);

	points->coords[2] = _x + _h;
	points->coords[3] = _y + _h;

	points->coords[4] = _x;
	points->coords[5] = _y;
	
	points->coords[6] = _x - _h;
	points->coords[7] = _y + _h;

	g_object_set (gobj(), "points", points, NULL);
}

void
Diamond::move_by (double dx, double dy)
{
	points->coords[0] += dx;
	points->coords[1] += dy;

	points->coords[2] += dx;
	points->coords[3] += dy;

	points->coords[4] += dx;
	points->coords[5] += dy;

	points->coords[6] += dx;
	points->coords[7] += dy;

	g_object_set (gobj(), "points", points, NULL);
}
