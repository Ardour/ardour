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
{
	points = gnome_canvas_points_new (4);
	g_object_set (gobj(), "points", points, NULL);
	set_height (height);
}

Diamond::~Diamond ()
{
	gnome_canvas_points_free (points);
}

void
Diamond::set_height(double height)
{
	double x1, y1, x2, y2; 
	
	get_bounds (x1, y1, x2, y2);

	points->coords[0] = x1;
	points->coords[1] = y1 + height*2.0;

	points->coords[2] = x2 + height;
	points->coords[3] = y1 + height;

	points->coords[4] = x1;
	points->coords[5] = y2;
	
	points->coords[6] = x2 -height;
	points->coords[7] = y2 + height;
	
	g_object_set (gobj(), "points", points, NULL);
}

