/*
 * Copyright (C) 2005-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
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

#ifndef __ardour_gtk_enums_h__
#define __ardour_gtk_enums_h__

#include "ardour/types.h"

enum Width {
	Wide,
	Narrow,
};

namespace ArdourCanvas {
	class Rectangle;
}

enum LayerDisplay {
	Overlaid,
	Stacked,
	Expanded
};

struct SelectionRect {
    ArdourCanvas::Rectangle *rect;
    ArdourCanvas::Rectangle *end_trim;
    ArdourCanvas::Rectangle *start_trim;
    uint32_t id;
};

enum Height {
	HeightLargest,
	HeightLarger,
	HeightLarge,
	HeightNormal,
	HeightSmall
};

extern void setup_gtk_ardour_enums ();

#endif /* __ardour_gtk_enums_h__ */
