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

	Color outline_color () const {
		return _outline_color;
	}

	virtual void set_outline_color (Color);

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

	Item&    _self;
	Color    _outline_color;
	Distance _outline_width;
	bool     _outline;
};

}

#endif
