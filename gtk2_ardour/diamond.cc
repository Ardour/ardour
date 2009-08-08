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
	set_height(height);
}

void
Diamond::set_height(double height)
{
	Points points;
	points.push_back(Art::Point(0, height*2.0));
	points.push_back(Art::Point(height, height));
	points.push_back(Art::Point(0, 0));
	points.push_back(Art::Point(-height, height));
	property_points() = points;	
}

