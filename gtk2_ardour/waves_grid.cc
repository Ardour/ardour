/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_grid.h"

WavesGrid::WavesGrid ()
{
}

WavesGrid::~WavesGrid()
{
}

void
WavesGrid::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::Fixed::on_size_allocate (alloc);
    int this_x = alloc.get_x ();
    int this_y = alloc.get_y ();
	int this_width = alloc.get_width ();
	int this_height = alloc.get_height ();
	int x = 0;
	int y = 0;
	int next_y = 0;

	std::vector<Gtk::Widget*> children = get_children ();

	for (std::vector<Gtk::Widget*>::iterator it = children.begin (); it != children.end (); ++it ) {
		Gtk::Widget& child = **it;
		Gtk::Allocation child_alloc = child.get_allocation ();
		
		if ( next_y < (y + child_alloc.get_height ())) {
			next_y = y + child_alloc.get_height ();
		}

		if (this_width < (x + child_alloc.get_width ())) {
			y = next_y;
			x = 0;
		}
        
		if ((x != (child_alloc.get_x () - this_x)) || (y != (child_alloc.get_y () - this_y))) {
           move (child, x, y);
        }
		x += child_alloc.get_width ();
	}
}

void
WavesGrid::pack (Gtk::Widget& widget)
{
	std::vector<Gtk::Widget*> children = get_children ();
	int x = 0;
	int y = 0;
	int width;
	int height;
	widget.get_size_request (width, height);

	if (!children.empty()) {
		Widget& last_child = *children.back ();
		Gtk::Allocation child_alloc =last_child.get_allocation ();
		int this_width;
		int this_height;
		get_size_request (this_width, this_height);
		
		y = child_alloc.get_y ();
		if (this_width <= (child_alloc.get_x () + child_alloc.get_width () + width)) {
			x = child_alloc.get_x () + child_alloc.get_width ();
		} else {
			y += height;
		}
	}
	put (widget, x, y);
}
