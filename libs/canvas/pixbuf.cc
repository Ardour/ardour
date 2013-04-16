/*
    Copyright (C) 2011-2013 Paul Davis
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

#include <cairomm/cairomm.h>
#include <gdkmm/general.h>

#include "canvas/pixbuf.h"

using namespace std;
using namespace ArdourCanvas;

Pixbuf::Pixbuf (Group* g)
	: Item (g)
{
	
}

void
Pixbuf::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	Gdk::Cairo::set_source_pixbuf (context, _pixbuf, 0, 0);
	context->paint ();
}
	
void
Pixbuf::compute_bounding_box () const
{
	if (_pixbuf) {
		_bounding_box = boost::optional<Rect> (Rect (0, 0, _pixbuf->get_width(), _pixbuf->get_height()));
	} else {
		_bounding_box = boost::optional<Rect> ();
	}

	_bounding_box_dirty = false;
}

void
Pixbuf::set (Glib::RefPtr<Gdk::Pixbuf> pixbuf)
{
	begin_change ();
	
	_pixbuf = pixbuf;
	_bounding_box_dirty = true;

	end_change ();
}

Glib::RefPtr<Gdk::Pixbuf>
Pixbuf::pixbuf() {
	return _pixbuf;
}

