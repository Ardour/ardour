/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_WIDGET_H__
#define __CANVAS_WIDGET_H__

#include "canvas/visibility.h"
#include "canvas/item.h"

class CairoWidget; /* should really be in Gtkmm2ext namespace */

namespace ArdourCanvas
{

class LIBCANVAS_API Widget : public Item
{
public:
	Widget (Canvas*, CairoWidget&);
	Widget (Item*, CairoWidget&);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void _size_allocate (Rect const &);

	CairoWidget const & get () const {
		return _widget;
	}

private:
	CairoWidget& _widget;
	bool event_proxy (GdkEvent*);
	bool queue_draw ();
	bool queue_resize ();
};

}

#endif
