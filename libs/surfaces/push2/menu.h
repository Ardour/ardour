/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_push2_menu_h__
#define __ardour_push2_menu_h__

namespace Cairo {
	class Context;
	class Region;
}

#include <pangomm/layout.h>

#include "pbd/signals.h"

#include "canvas/container.h"

namespace ArdourCanvas {
	class Text;
	class Rectangle;
}

namespace ArdourSurface {

class Push2Menu : public ArdourCanvas::Container
{
   public:
	Push2Menu (ArdourCanvas::Item* parent);

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void fill_column (int col, std::vector<std::string>);
	void set_active (int col, int index);
	void step_active (int col, int dir);
	int get_active (int col);

	PBD::Signal0<void> ActiveChanged;
	PBD::Signal0<void> Selected;

   private:
	struct Column {
		std::vector<std::string> text;
		ArdourCanvas::Rectangle* active_bg;
		ArdourCanvas::Text* lines;
		int top;
		int active;
	};

	Column  columns[8];

	void scroll (int col, int dir);
	void set_text (int col, int top);

	int nrows;
	mutable double baseline;
};

} // namespace

#endif /* __ardour_push2_menu_h__ */
