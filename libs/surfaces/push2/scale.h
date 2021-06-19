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

class Push2Menu;

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
	ArdourCanvas::Rectangle* _bg;
	std::vector<ArdourCanvas::Text*> _upper_text;
	std::vector<ArdourCanvas::Text*> _lower_text;
	ArdourCanvas::Text* _left_scroll_text;
	ArdourCanvas::Text* _right_scroll_text;
	ArdourCanvas::Text* _inkey_text;
	ArdourCanvas::Text* _chromatic_text;
	ArdourCanvas::Text* _fixed_text;
	ArdourCanvas::Text* _rooted_text;
	ArdourCanvas::Text* _row_interval_text;
	ArdourCanvas::Text* _close_text;
	Push2Menu* _scale_menu;
	int _last_vpot;
	int _vpot_delta_cnt;
	boost::shared_ptr<Push2::Button> _root_button;

	void build_scale_menu ();
	PBD::ScopedConnectionList _menu_connections;
	PBD::ScopedConnectionList _p2_connections;
	void mode_changed ();
	void menu_rearranged ();
	void show_root_state ();
	void show_fixed_state ();
	void update_cursor_buttons ();
};

} /* namespace */

#endif /* __ardour_push2_scale_layout_h__ */
