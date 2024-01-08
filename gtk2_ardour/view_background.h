/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_view_background_h__
#define __gtk2_ardour_view_background_h__

#include <cstdint>

#include "gtkmm2ext/colors.h"

namespace ArdourCanvas {
	class Item;
}

/** A class that provides limited context for a View
 */

class ViewBackground
{
  public:
	ViewBackground ();
	virtual ~ViewBackground ();

	virtual double contents_height() const { return 0.; }

	/** @return y position, or -1 if hidden */
	virtual double y_position () const { return 0.; }

  protected:
	virtual void update_contents_height () {}
	virtual void color_handler () {}
	virtual void parameter_changed (std::string const &) {}
};


#endif /* __gtk2_ardour_view_background_h__ */
