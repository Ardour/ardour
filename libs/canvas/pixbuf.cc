/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 Robin Gareus <robin@gareus.org>
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

#include <cairomm/cairomm.h>
#include <gdkmm/general.h>

#include "canvas/pixbuf.h"

using namespace std;
using namespace ArdourCanvas;

Pixbuf::Pixbuf (Canvas* c)
	: Item (c)
{
}

Pixbuf::Pixbuf (Item* parent)
	: Item (parent)
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
		_bounding_box = Rect (Rect (0, 0, _pixbuf->get_width(), _pixbuf->get_height()));
	} else {
		_bounding_box = Rect ();
	}

	bb_clean ();
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

