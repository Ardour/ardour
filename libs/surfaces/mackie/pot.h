#ifndef __ardour_mackie_control_protocol_pot_h__
#define __ardour_mackie_control_protocol_pot_h__

#include "controls.h"
#include "ledring.h"

namespace Mackie {

class Pot : public Control
{
public:
	Pot (int id, int ordinal, std::string name, Group & group)
		: Control (id, ordinal, name, group)
		, _led_ring (id, ordinal, name + "_ring", group) {}

	virtual type_t type() const { return type_pot; }

	virtual const LedRing & led_ring() const {return _led_ring; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);

private:
	LedRing _led_ring;
};

}

#endif /* __ardour_mackie_control_protocol_pot_h__ */
