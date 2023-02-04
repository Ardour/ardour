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

#ifndef __CANVAS_FILL_H__
#define __CANVAS_FILL_H__

#include <vector>
#include <stdint.h>

#include <boost/noncopyable.hpp>

#include "canvas/visibility.h"
#include "canvas/types.h"

namespace ArdourCanvas {

class Item;

class LIBCANVAS_API Fill : public boost::noncopyable
{
public:
	Fill (Item& self);
	virtual ~Fill() {}

	virtual void set_fill_color (Gtkmm2ext::Color);
	virtual void set_fill (bool);

	Gtkmm2ext::Color fill_color () const {
		return _fill_color;
	}

	bool fill () const {
		return _fill;
	}

	typedef std::vector<std::pair<double,Gtkmm2ext::Color> > StopList;

	void set_gradient (StopList const & stops, bool is_vertical);

	void set_pattern (Cairo::RefPtr<Cairo::Pattern>);

protected:
	void setup_fill_context (Cairo::RefPtr<Cairo::Context>) const;
	void setup_gradient_context (Cairo::RefPtr<Cairo::Context>, Rect const &, Duple const &) const;

	Item& _self;
	Gtkmm2ext::Color _fill_color;
	bool _fill;
	bool _transparent;
	StopList _stops;
	bool _vertical_gradient;
	Cairo::RefPtr<Cairo::Pattern> _pattern;

};

}

#endif
