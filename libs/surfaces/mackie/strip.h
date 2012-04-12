#ifndef __ardour_mackie_control_protocol_strip_h__
#define __ardour_mackie_control_protocol_strip_h__

#include <string>
#include <iostream>

#include "pbd/property_basics.h"
#include "pbd/signals.h"

#include "control_protocol/types.h"

#include "control_group.h"
#include "types.h"
#include "midi_byte_array.h"

namespace ARDOUR {
	class Route;
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
	Strip (Surface&, const std::string & name, int index, StripControlDefinition* ctls);
	~Strip();

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }

	void add (Control & control);
	int index() const { return _index; } // zero based
	
	void set_route (boost::shared_ptr<ARDOUR::Route>);

	// call all signal handlers manually
	void notify_all();

	void handle_button (Button&, ButtonState bs);
	void handle_fader (Fader&, float position);
	void handle_pot (Pot&, float delta);

	void periodic ();

	MidiByteArray display (uint32_t line_number, const std::string&);
	MidiByteArray blank_display (uint32_t line_number);
	MidiByteArray zero ();

	void lock_route ();
	void unlock_route ();
	
	MidiByteArray gui_selection_changed (ARDOUR::RouteNotificationListPtr);

private:
	Button* _solo;
	Button* _recenable;
	Button* _mute;
	Button* _select;
	Button* _vselect;
	Button* _fader_touch;
	Pot*    _vpot;
	Fader*  _fader;
	Meter*  _meter;
	int     _index;
	Surface* _surface;
	bool     _route_locked;

	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;

	float _last_fader_position_written;
	float _last_vpot_position_written;

	void notify_solo_changed ();
	void notify_mute_changed ();
	void notify_record_enable_changed ();
	void notify_gain_changed (bool force_update = true);
	void notify_property_changed (const PBD::PropertyChange&);
	void notify_panner_changed (bool force_update = true);
	void notify_active_changed ();
	void notify_route_deleted ();
	
	void update_automation ();
	void update_meter ();
};

}

#endif /* __ardour_mackie_control_protocol_strip_h__ */
