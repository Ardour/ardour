#ifndef __ardour_mackie_control_protocol_fader_h__
#define __ardour_mackie_control_protocol_fader_h__

#include "controls.h"

namespace Mackie {

class Fader : public Control
{
  public:
	
	Fader (int id, std::string name, Group & group)
		: Control (id, name, group)
		, position (0.0)
	{
	}

	MidiByteArray set_position (float);
	MidiByteArray zero() { return set_position (0.0); }
	
	MidiByteArray update_message ();

	static Control* factory (Surface&, int id, const char*, Group&);
	
  private:
	float position;
};

}

#endif
