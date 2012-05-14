#ifndef __ardour_mackie_control_protocol_strip_h__
#define __ardour_mackie_control_protocol_strip_h__

#include <string>
#include <iostream>

#include "evoral/Parameter.hpp"

#include "pbd/property_basics.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "control_protocol/types.h"

#include "control_group.h"
#include "types.h"
#include "midi_byte_array.h"
#include "device_info.h"

namespace ARDOUR {
	class Route;
	class Bundle;
	class ChannelCount;
}

namespace Mackie {

class Control;
class Surface;
class Button;
class Pot;
class Fader;
class Meter;
class SurfacePort;

struct StripControlDefinition {
    const char* name;
    uint32_t base_id;
    Control* (*factory)(Surface&, int index, const char* name, Group&);
};

struct GlobalControlDefinition {
    const char* name;
    uint32_t id;
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

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }

	void add (Control & control);
	int index() const { return _index; } // zero based
	
	void set_route (boost::shared_ptr<ARDOUR::Route>, bool with_messages = true);

	// call all signal handlers manually
	void notify_all();

	void handle_button (Button&, ButtonState bs);
	void handle_fader (Fader&, float position);
	void handle_pot (Pot&, float delta);

	void periodic (uint64_t now_usecs);

	MidiByteArray display (uint32_t line_number, const std::string&);
	MidiByteArray blank_display (uint32_t line_number);

	void zero ();

	void flip_mode_changed (bool notify=false);

	void lock_controls ();
	void unlock_controls ();
	bool locked() const { return _controls_locked; }

	void gui_selection_changed (const ARDOUR::StrongRouteNotificationList&);

private:
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
	uint64_t _reset_display_at;
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;

	float _last_gain_position_written;
	float _last_pan_azi_position_written;
	float _last_pan_width_position_written;

	void notify_solo_changed ();
	void notify_mute_changed ();
	void notify_record_enable_changed ();
	void notify_gain_changed (bool force_update = true);
	void notify_property_changed (const PBD::PropertyChange&);
	void notify_panner_azi_changed (bool force_update = true);
	void notify_panner_width_changed (bool force_update = true);
	void notify_active_changed ();
	void notify_route_deleted ();
	
	void update_automation ();
	void update_meter ();

	std::string vpot_mode_string () const;

	void queue_display_reset (uint32_t msecs);
	void clear_display_reset ();
	void reset_display ();
	void do_parameter_display (ARDOUR::AutomationType, float val);
	
	typedef std::map<std::string,boost::shared_ptr<ARDOUR::Bundle> > BundleMap;
	BundleMap input_bundles;
	BundleMap output_bundles;

	void build_input_list (const ARDOUR::ChanCount&);
	void build_output_list (const ARDOUR::ChanCount&);
	void maybe_add_to_bundle_map (BundleMap& bm, boost::shared_ptr<ARDOUR::Bundle>, bool for_input, const ARDOUR::ChanCount&);

	void select_event (Button&, ButtonState);
	void vselect_event (Button&, ButtonState);
	void fader_touch_event (Button&, ButtonState);

	std::vector<Evoral::Parameter> possible_pot_parameters;
	void next_pot_mode ();
	void set_vpot_parameter (Evoral::Parameter);

	void reset_saved_values ();

	std::map<Evoral::Parameter,Control*> control_by_parameter;
};

}

#endif /* __ardour_mackie_control_protocol_strip_h__ */
