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

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <pangomm/layout.h>

#include "pbd/signals.h"

namespace ArdourSurface {

class Push2Menu {
   public:
	Push2Menu (Cairo::RefPtr<Cairo::Context>);

	void redraw (Cairo::RefPtr<Cairo::Context>) const;
	bool dirty () const { return _dirty; }

	void fill_column (int col, std::vector<std::string>);
	void set_active (int col, int index);
	void step_active (int col, int dir);
	int get_active (int col);

	PBD::Signal0<void> ActiveChanged;
	PBD::Signal0<void> Selected;

   private:
	struct Column {
		std::vector<std::string> text;
		Glib::RefPtr<Pango::Layout> layout;
		int top;
		int active;
	};

	Column  columns[8];

	void scroll (int col, int dir);
	void set_text (int col, int top);

	int nrows;
	double baseline;

	mutable bool _dirty;
};

} // namespace

#endif /* __ardour_push2_menu_h__ */
