/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_enums_h__
#define __ardour_gtk_enums_h__

#include "ardour/types.h"

enum Width {
	Wide,
	Narrow,
};

namespace Gnome {
	namespace Canvas {
		class SimpleRect;
	}
}

enum LayerDisplay {
	Overlaid,
	Stacked,
	Expanded
};

struct SelectionRect {
    Gnome::Canvas::SimpleRect *rect;
    Gnome::Canvas::SimpleRect *end_trim;
    Gnome::Canvas::SimpleRect *start_trim;
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
