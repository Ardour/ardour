/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_scale_layout_h__
#define __ardour_push2_scale_layout_h__

#include <vector>

#include "layout.h"

namespace ArdourCanvas {
	class Rectangle;
}

namespace ArdourSurface {

class ScaleLayout : public Push2Layout
{
   public:
	ScaleLayout (Push2& p, ARDOUR::Session&, std::string const &);
	~ScaleLayout ();

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

	void show ();

	void button_upper (uint32_t n);
	void button_lower (uint32_t n);
	void button_up ();
	void button_down ();
	void button_left ();
	void button_right ();
	void strip_vpot (int, int);

	void strip_vpot_touch (int, bool) {}

   private:
	ArdourCanvas::Rectangle* bg;
	std::vector<ArdourCanvas::Text*> upper_text;
	std::vector<ArdourCanvas::Text*> lower_text;
	ArdourCanvas::Text* left_scroll_text;
	ArdourCanvas::Text* right_scroll_text;
	ArdourCanvas::Text* inkey_text;
	ArdourCanvas::Text* chromatic_text;
	ArdourCanvas::Text* close_text;
	Push2Menu* scale_menu;
	int last_vpot;
	int vpot_delta_cnt;
	boost::shared_ptr<Push2::Button> root_button;

	void build_scale_menu ();
	PBD::ScopedConnectionList menu_connections;
	PBD::ScopedConnectionList p2_connections;
	void mode_changed ();
	void menu_rearranged ();
	void show_root_state ();
	void update_cursor_buttons ();
};

} /* namespace */

#endif /* __ardour_push2_scale_layout_h__ */
