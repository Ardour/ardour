/*
    Copyright (C) 2011-2014 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include "canvas/container.h"

using namespace ArdourCanvas;

Container::Container (Canvas* canvas) 
	: Item (canvas)
{
}

Container::Container (Item* parent) 
	: Item (parent)
{
}


Container::Container (Item* parent, Duple const & p) 
	: Item (parent, p)
{
}

void
Container::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Item::render_children (area, context);
}

void
Container::compute_bounding_box () const
{
	_bounding_box = boost::none;
	add_child_bounding_boxes ();
	_bounding_box_dirty = false;
}

