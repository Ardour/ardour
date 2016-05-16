#ifndef __ardour_mackie_control_protocol_strip_h__
#define __ardour_mackie_control_protocol_strip_h__

#include <string>
#include <iostream>

#include "evoral/Parameter.hpp"

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

	void periodic (ARDOUR::microseconds_t now_usecs);
	void redisplay (ARDOUR::microseconds_t now_usecs, bool force = true);

	MidiByteArray display (uint32_t line_number, const std::string&);
	MidiByteArray blank_display (uint32_t line_number);

	void zero ();

	void flip_mode_changed ();
	void subview_mode_changed ();

	void lock_controls ();
	void unlock_controls ();
	bool locked() const { return _controls_locked; }

	void gui_selection_changed (const ARDOUR::StrongStripableNotificationList&);

	void notify_metering_state_changed();

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
	std::string pending_display[2];
	std::string current_display[2];
	uint64_t _block_screen_redisplay_until;
	uint64_t return_to_vpot_mode_display_at;
	boost::shared_ptr<ARDOUR::Stripable> _stripable;
	PBD::ScopedConnectionList stripable_connections;
	PBD::ScopedConnectionList subview_connections;
	PBD::ScopedConnectionList send_connections;
	int       eq_band;

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
	void notify_active_changed ();
	void notify_stripable_deleted ();
	void notify_processor_changed (bool force_update = true);
	void update_automation ();
	void update_meter ();
	std::string vpot_mode_string ();

	boost::shared_ptr<ARDOUR::AutomationControl> mb_pan_controllable;

	void return_to_vpot_mode_display ();
	void next_pot_mode ();

	void do_parameter_display (ARDOUR::AutomationType, float val);
	void select_event (Button&, ButtonState);
	void vselect_event (Button&, ButtonState);
	void fader_touch_event (Button&, ButtonState);

	std::vector<ARDOUR::AutomationType> possible_pot_parameters;
	std::vector<ARDOUR::AutomationType> possible_trim_parameters;
	void set_vpot_parameter (ARDOUR::AutomationType);
	void show_stripable_name ();

	void reset_saved_values ();

	bool is_midi_track () const;

	void notify_eq_change (ARDOUR::AutomationType, uint32_t band, bool force);
	void setup_eq_vpot (boost::shared_ptr<ARDOUR::Stripable>);

	void notify_dyn_change (ARDOUR::AutomationType, bool force, bool propagate_mode_change);
	void setup_dyn_vpot (boost::shared_ptr<ARDOUR::Stripable>);

	void notify_send_level_change (ARDOUR::AutomationType, uint32_t band, bool force);
	void setup_sends_vpot (boost::shared_ptr<ARDOUR::Stripable>);

	void notify_trackview_change (ARDOUR::AutomationType, uint32_t band, bool force);
	void setup_trackview_vpot (boost::shared_ptr<ARDOUR::Stripable>);
};

}
}

#endif /* __ardour_mackie_control_protocol_strip_h__ */
