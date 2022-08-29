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
#include "push2.h"

namespace ARDOUR {
	class Route;
	class AutomationControl;
}

namespace ArdourCanvas {
	struct Rect;

	class Rectangle;
	class Text;
	class Line;
	class Arc;
}

namespace ArdourSurface {

class Push2Knob;
class LevelMeter;

class CueLayout : public Push2Layout
{
   public:
	/* Possible knob functions */
	enum KnobFunction {
		KnobGain,
		KnobPan,
		KnobSendA,
		KnobSendB,
	};

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
	void button_stop_release ();
	void button_stop_long_press ();

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

	void pad_press (int x, int y);

	/* override to use for clip progress */
	void update_meters();

   private:
	ArdourCanvas::Rectangle*         _bg;
	ArdourCanvas::Line*              _upper_line;
	std::vector<ArdourCanvas::Text*> _upper_text;
	std::vector<ArdourCanvas::Rectangle*> _upper_backgrounds;
	std::vector<ArdourCanvas::Text*> _lower_text;
	uint8_t                          _selection_color;
	uint32_t                         track_base;
	uint32_t                         scene_base;
	KnobFunction                     _knob_function;
	int                              _long_stop;

	PBD::ScopedConnectionList        _route_connections;
	boost::shared_ptr<ARDOUR::Route> _route[8];
	PBD::ScopedConnectionList        _session_connections;
	PBD::ScopedConnection            _trig_connections[64];

	void routes_added ();
	void route_property_change (PBD::PropertyChange const& what_changed, uint32_t which);
	void triggerbox_property_change (PBD::PropertyChange const& what_changed, uint32_t which);
	void trigger_property_change (PBD::PropertyChange const& what_changed, uint32_t col, uint32_t row);

	ArdourCanvas::Arc* _progress[8];
	boost::shared_ptr<ARDOUR::AutomationControl> _controllables[8];

	void viewport_changed ();

	void show_state ();
	void update_clip_progress (int);
	void show_knob_function ();
	void set_pad_color_from_trigger_state (int col, boost::shared_ptr<Push2::Pad>, boost::shared_ptr<ARDOUR::Trigger>);
	void show_running_boxen (bool);
};

} /* namespace */

#endif /* __ardour_push2_clip_view_h__ */
