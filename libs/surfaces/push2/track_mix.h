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

#ifndef __ardour_push2_track_mix_layout_h__
#define __ardour_push2_track_mix_layout_h__

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

class TrackMixLayout : public Push2Layout
{
   public:
	TrackMixLayout (Push2& p, ARDOUR::Session&, std::string const &);
	~TrackMixLayout ();

	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

	void show ();
	void hide ();
	void button_upper (uint32_t n);
	void button_lower (uint32_t n);
	void button_left ();
	void button_right ();

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

	void update_meters ();
	void update_clocks ();

	boost::shared_ptr<ARDOUR::Stripable> current_stripable() const { return stripable; }

   private:
	boost::shared_ptr<ARDOUR::Stripable> stripable;
	PBD::ScopedConnectionList stripable_connections;

	ArdourCanvas::Rectangle* bg;
	ArdourCanvas::Line* upper_line;
	std::vector<ArdourCanvas::Text*> upper_text;
	std::vector<ArdourCanvas::Text*> lower_text;
	ArdourCanvas::Text* name_text;
	ArdourCanvas::Text* bbt_text;
	ArdourCanvas::Text* minsec_text;
	uint8_t selection_color;

	Push2Knob* knobs[8];
	LevelMeter* meter;

	void stripable_property_change (PBD::PropertyChange const& what_changed);
	void simple_control_change (boost::shared_ptr<ARDOUR::AutomationControl> ac, Push2::ButtonID bid);

	void show_state ();

	void drop_stripable ();
	void name_changed ();
	void color_changed ();

	void solo_mute_change ();
	void rec_enable_change ();
	void solo_iso_change ();
	void solo_safe_change ();
	void monitoring_change ();
};

} /* namespace */

#endif /* __ardour_push2_track_mix_layout_h__ */
