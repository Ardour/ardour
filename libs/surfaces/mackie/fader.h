#ifndef __ardour_mackie_control_protocol_fader_h__
#define __ardour_mackie_control_protocol_fader_h__

#include "controls.h"

namespace Mackie {

class Fader : public Control
{
public:
	Fader (int id, int ordinal, std::string name, Group & group)
		: Control (id, ordinal, name, group)
	{
	}
	
	virtual type_t type() const { return type_fader; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
};

}

#endif
