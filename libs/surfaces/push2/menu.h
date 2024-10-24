/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_menu_h__
#define __ardour_push2_menu_h__

#include <vector>

namespace Cairo {
	class Context;
}

#include <pangomm/fontdescription.h>

#include "pbd/signals.h"

#include "canvas/container.h"

namespace ArdourCanvas {
	class Text;
	class Rectangle;
	class Rect;
}

namespace ArdourSurface {

class Push2Menu : public ArdourCanvas::Container
{
   public:
	Push2Menu (ArdourCanvas::Item* parent, std::vector<std::string>);

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void set_wrap (bool);
	void set_active (uint32_t index);

	uint32_t active () const { return _active; }
	uint32_t items() const { return _displays.size(); }

	uint32_t rows() const { return _nrows; }
	uint32_t cols() const { return _ncols; }

	void set_layout (int cols, int rows);
	void set_font_description (Pango::FontDescription);
	void set_text_color (Gtkmm2ext::Color);
	void set_active_color (Gtkmm2ext::Color);

	bool can_scroll_left() const { return _first >= _nrows; }
	bool can_scroll_right() const { return _last < _displays.size() - 1; }

	enum Direction { DirectionUp, DirectionDown, DirectionLeft, DirectionRight };
	void scroll (Direction, bool page = false);

	PBD::Signal<void()> ActiveChanged;
	PBD::Signal<void()> Rearranged;

   private:
	std::vector<ArdourCanvas::Text*> _displays;
	ArdourCanvas::Rectangle*         _active_bg;

	void rearrange (uint32_t initial_display);

	double   _baseline;
	int      _row_start;
	int      _col_start;
	uint32_t _ncols;
	uint32_t _nrows;
	bool     _wrap;
	uint32_t _first;
	uint32_t _last;
	uint32_t _active;

	Gtkmm2ext::Color       _text_color;
	Gtkmm2ext::Color       _active_color;
	Gtkmm2ext::Color       _contrast_color;
	Pango::FontDescription _font_description;

	inline int active_row () const { return _active % _nrows; }
	inline int active_col () const { return (_active / _nrows); }
	inline int active_top () const { return active_col () * _nrows; }
};

} // namespace

#endif /* __ardour_push2_menu_h__ */
