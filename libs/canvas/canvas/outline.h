/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_OUTLINE_H__
#define __CANVAS_OUTLINE_H__

#include <stdint.h>

#include <boost/noncopyable.hpp>

#include "canvas/visibility.h"
#include "canvas/types.h"

namespace ArdourCanvas {

class Item;

class LIBCANVAS_API Outline : public boost::noncopyable
{
public:
	Outline (Item& self);
	virtual ~Outline() {}

	Gtkmm2ext::Color outline_color () const {
		return _outline_color;
	}

	virtual void set_outline_color (Gtkmm2ext::Color);

	Distance outline_width () const {
		return _outline_width;
	}

	virtual void set_outline_width (Distance);

	bool outline () const {
		return _outline;
	}

	virtual void set_outline (bool);

protected:

	void setup_outline_context (Cairo::RefPtr<Cairo::Context>) const;

	Item&            _self;
	Gtkmm2ext::Color _outline_color;
	Distance         _outline_width;
	bool             _outline;
};

}

#endif
