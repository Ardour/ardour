#ifndef __ardour_mackie_control_protocol_pot_h__
#define __ardour_mackie_control_protocol_pot_h__

#include "controls.h"

namespace Mackie {

class Pot : public Control
{
public:
	enum base_id_t {
		base_id = 0x10,
	};

	enum Mode {
		dot = 0,
		boost_cut = 1,
		wrap = 2,
		spread = 3,
	};

	Pot (int id, std::string name, Group & group)
		: Control (id, name, group)
		, value (0.0)
		, mode (dot)
		, on (true) {}

	MidiByteArray set_mode (Mode);
	MidiByteArray set_value (float);
	MidiByteArray set_onoff (bool);
	MidiByteArray set_all (float, bool, Mode);

	MidiByteArray zero() { return set_value (0.0); }
	
	MidiByteArray update_message ();

	static Control* factory (Surface&, int id, const char*, Group&);

  private:
	float value;
	Mode mode;
	bool on;
};

}

#endif /* __ardour_mackie_control_protocol_pot_h__ */
