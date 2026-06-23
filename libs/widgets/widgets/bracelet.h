/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <vector>

#include "gtkmm2ext/cairo_widget.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API Bracelet : public CairoWidget {
public:
	Bracelet (int points);
	virtual ~Bracelet ();

	void fill (int step);
	void fill (std::vector<int> const & steps);
	void clear ();

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);

	void set_fill_color (int);
	void set_outline_color (int);

protected:
	void on_size_allocate (Gtk::Allocation&);

private:
	int _steps;
	std::vector<bool> active;
	float width;
	int fill_color;
	int outline_color;
};


} /* namespace */
