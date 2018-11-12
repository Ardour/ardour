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

#ifndef __CANVAS_UTILS_H__
#define __CANVAS_UTILS_H__

#include <cairomm/context.h>

#include "canvas/visibility.h"
#include "canvas/types.h"

namespace ArdourCanvas {

	Distance LIBCANVAS_API distance_to_segment_squared (Duple const & p, Duple const & p1, Duple const & p2, double& t, Duple& at);
}

#endif // __CANVAS_UTILS_H__
