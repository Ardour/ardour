#ifndef __ardour_us2400_control_protocol_fader_h__
#define __ardour_us2400_control_protocol_fader_h__

#include "controls.h"

namespace ArdourSurface {

namespace US2400 {

class Fader : public Control
{
  public:

	Fader (int id, std::string name, Group & group)
		: Control (id, name, group)
		, position (0.0)
		, last_update_position (-1)
		, llast_update_position (-1)
	{
	}

	MidiByteArray set_position (float);
	MidiByteArray zero() { return set_position (0.0); }

	MidiByteArray update_message ();

	static Control* factory (Surface&, int id, const char*, Group&);

	void mark_dirty() { last_update_position = llast_update_position = -1; }

  private:
	float position;
	int   last_update_position;
	int   llast_update_position;
};

}
}

#endif
