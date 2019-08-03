/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_PIXBUF__
#define __CANVAS_PIXBUF__

#include <glibmm/refptr.h>

#include "canvas/visibility.h"
#include "canvas/item.h"

namespace Gdk {
	class Pixbuf;
}

namespace ArdourCanvas {

class LIBCANVAS_API Pixbuf : public Item
{
public:
	Pixbuf (Canvas*);
	Pixbuf (Item*);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set (Glib::RefPtr<Gdk::Pixbuf>);

	/* returns the reference to the internal private pixbuf
	 * after changing data in the pixbuf a call to set()
	 * is mandatory to update the data on screen */
	Glib::RefPtr<Gdk::Pixbuf> pixbuf();

private:
	Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
};

}
#endif
