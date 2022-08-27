/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_clip_view_h__
#define __ardour_push2_clip_view_h__

#include <vector>

#include "layout.h"

namespace ARDOUR {
	class Stripable;
	class AutomationControl;
}

namespace ArdourCanvas {
	struct Rect;

	class Rectangle;
	class Text;
	class Line;
}

namespace ArdourSurface {

class Push2Knob;
class LevelMeter;

class CueLayout : public Push2Layout
{
   public:
	CueLayout (Push2& p, ARDOUR::Session&, std::string const &);
	~CueLayout ();

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

	void show ();
	void hide ();
	void button_upper (uint32_t n);
	void button_lower (uint32_t n);
	void button_left ();
	void button_right ();
	void button_up();
	void button_down ();
	void button_rhs (int);
	void button_octave_up();
	void button_octave_down();
	void button_page_left();
	void button_page_right();
	void button_stop_press ();

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

	void pad_press (int x, int y);

   private:
	ArdourCanvas::Rectangle*         _bg;
	ArdourCanvas::Line*              _upper_line;
	std::vector<ArdourCanvas::Text*> _upper_text;
	std::vector<ArdourCanvas::Text*> _lower_text;
	uint8_t                          _selection_color;
	uint32_t                         track_base;
	uint32_t                         scene_base;

	Push2Knob*  _knobs[8];

	void show_state ();
};

} /* namespace */

#endif /* __ardour_push2_clip_view_h__ */
