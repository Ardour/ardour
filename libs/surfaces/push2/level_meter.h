/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_push2_level_meter_h__
#define __ardour_push2_level_meter_h__

#include "canvas/container.h"
#include "canvas/meter.h"

namespace ARDOUR {
	class PeakMeter;
}

namespace ArdourCanvas {
	class Box;
}

namespace ArdourSurface
{

class Push2;

class LevelMeter : public ArdourCanvas::Container, public sigc::trackable
{
  public:
	LevelMeter (Push2& p2, Item* parent, int len, ArdourCanvas::Meter::Orientation o = ArdourCanvas::Meter::Vertical);
	virtual ~LevelMeter ();

	virtual void set_meter (ARDOUR::PeakMeter* meter);

	void update_gain_sensitive ();

	float update_meters ();
	void update_meters_falloff ();
	void clear_meters (bool reset_highlight = true);
	void hide_meters ();
	void setup_meters (int len=0, int width=3, int thin=2);
	void set_max_audio_meter_count (uint32_t cnt = 0);


  private:
	Push2& p2;
	ARDOUR::PeakMeter* _meter;
	ArdourCanvas::Meter::Orientation _meter_orientation;
	ArdourCanvas::Box* meter_packer;

	struct MeterInfo {
		ArdourCanvas::Meter* meter;
		gint16 width;
		int    length;
		bool   packed;
		float  max_peak;

		MeterInfo() {
			meter = 0;
			width = 0;
			length = 0;
			packed = false;
			max_peak = -INFINITY;
		}
	};

	guint16                regular_meter_width;
	int                    meter_length;
	guint16                thin_meter_width;
	std::vector<MeterInfo> meters;
	float                  max_peak;
	ARDOUR::MeterType      visible_meter_type;
	uint32_t               midi_count;
	uint32_t               meter_count;
	uint32_t               max_visible_meters;

	PBD::ScopedConnection _configuration_connection;
	PBD::ScopedConnection _meter_type_connection;
	PBD::ScopedConnection _parameter_connection;

	void hide_all_meters ();

	void parameter_changed (std::string);
	void configuration_changed (ARDOUR::ChanCount in, ARDOUR::ChanCount out);
	void meter_type_changed (ARDOUR::MeterType);
};

}

#endif /* __ardour_push2_level_meter_h__ */
