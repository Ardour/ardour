/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_cairo_canvas_h__
#define __gtk2_ardour_cairo_canvas_h__

#include <stdint.h>
#include <cairomm/context.h>
#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API CairoCanvas
{
public:
	virtual ~CairoCanvas () {}

	virtual void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*) = 0;
	virtual uint32_t background_color () = 0;
};

} /* namespace */
#endif
