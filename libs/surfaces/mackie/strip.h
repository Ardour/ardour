/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_mackie_control_protocol_strip_h__
#define __ardour_mackie_control_protocol_strip_h__

#include <string>
#include <iostream>

#include "evoral/Parameter.h"

#include "pbd/property_basics.h"
#include "pbd/ringbuffer.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "control_protocol/types.h"

#include "control_group.h"
#include "types.h"
#include "mackie_control_protocol.h"
#include "midi_byte_array.h"
#include "device_info.h"

namespace ARDOUR {
	class Stripable;
	class Bundle;
	class ChannelCount;
}

namespace ArdourSurface {

namespace Mackie {

class Control;
class Surface;
class Button;
class Pot;
class Fader;
class Meter;
class SurfacePort;

struct GlobalControlDefinition {
    const char* name;
    int id;
    Control* (*factory)(Surface&, int index, const char* name, Group&);
    const char* group_name;
};

/**
	This is the set of controls that make up a strip.
*/
class Strip : public Group
{
public:
	Strip (Surface&, const std::string & name, int index, const std::map<Button::ID,StripButtonInfo>&);
	~Strip();

	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return _stripable; }

	void add (Control & control);
	int index() const { return _index; } // zero based
	Surface* surface() const { return _surface; }

	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>, bool with_messages = true);

	// call all signal handlers manually
	void notify_all ();

	void handle_button (Button&, ButtonState bs);
	void handle_fader (Fader&, float position);
	void handle_fader_touch (Fader&, bool touch_on);
	void handle_pot (Pot&, float delta);

	void periodic (PBD::microseconds_t now_usecs);
	void redisplay (PBD::microseconds_t now_usecs, bool force = true);

	MidiByteArray display (uint32_t lcd_number, uint32_t line_number, const std::string&);
	MidiByteArray blank_display (uint32_t lcd_number, uint32_t line_number);
	
	static std::string format_parameter_for_display(
		ARDOUR::ParameterDescriptor const& desc, 
		float val, 
		boost::shared_ptr<ARDOUR::Stripable> stripable_for_non_mixbus_azimuth_automation, 
		bool& overwrite_screen_hold);

	void zero ();

	void flip_mode_changed ();
	void subview_mode_changed ();

	void lock_controls ();
	void unlock_controls ();
	bool locked() const { return _controls_locked; }

	void notify_metering_state_changed();

	void update_selection_state ();

	void block_screen_display_for (uint32_t msecs);
	void block_vpot_mode_display_for (uint32_t msecs);

private:
	enum VPotDisplayMode {
		Name,
		Value
	};

	Button*  _solo;
	Button*  _recenable;
	Button*  _mute;
	Button*  _select;
	Button*  _vselect;
	Button*  _fader_touch;
	Pot*     _vpot;
	Fader*   _fader;
	Meter*   _meter;
	int      _index;
	Surface* _surface;
	bool     _controls_locked;
	bool     _transport_is_rolling;
	bool     _metering_active;
	bool     _lcd2_available;
	uint32_t _lcd2_label_pitch;				// number of label characters including the required space between strips
	std::string pending_display[2];
	std::string current_display[2];
	std::string lcd2_pending_display[2];
	std::string lcd2_current_display[2];
	PBD::microseconds_t _block_screen_redisplay_until;
	PBD::microseconds_t return_to_vpot_mode_display_at;
	boost::shared_ptr<ARDOUR::Stripable> _stripable;
	PBD::ScopedConnectionList stripable_connections;

	ARDOUR::AutomationType  _pan_mode;

	float _last_gain_position_written;
	float _last_pan_azi_position_written;
	float _last_pan_width_position_written;
	float _last_trim_position_written;

	void notify_solo_changed ();
	void notify_mute_changed ();
	void notify_record_enable_changed ();
	void notify_gain_changed (bool force_update = true);
	void notify_property_changed (const PBD::PropertyChange&);
	void notify_panner_azi_changed (bool force_update = true);
	void notify_panner_width_changed (bool force_update = true);
	void notify_stripable_deleted ();
	void notify_processor_changed (bool force_update = true);
	void update_automation ();
	void update_meter ();
	std::string vpot_mode_string ();

	boost::shared_ptr<ARDOUR::AutomationControl> mb_pan_controllable;

	void return_to_vpot_mode_display ();
	void next_pot_mode ();

	void do_parameter_display (ARDOUR::ParameterDescriptor const&, float val, bool screen_hold = false);
	void select_event (Button&, ButtonState);
	void vselect_event (Button&, ButtonState);
	void fader_touch_event (Button&, ButtonState);

	std::vector<ARDOUR::AutomationType> possible_pot_parameters;
	std::vector<ARDOUR::AutomationType> possible_trim_parameters;
	void set_vpot_parameter (ARDOUR::AutomationType);
	void show_stripable_name ();

	void reset_saved_values ();

	bool is_midi_track () const;
};

}
}

#endif /* __ardour_mackie_control_protocol_strip_h__ */
