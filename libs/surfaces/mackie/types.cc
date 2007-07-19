#include "types.h"

namespace Mackie
{
LedState on( LedState::on );
LedState off( LedState::off );
LedState flashing( LedState::flashing );
LedState none( LedState::none );

std::ostream & operator << ( std::ostream & os, const ControlState & cs )
{
	os << "ControlState { ";
	os << "pos: " << cs.pos;
	os << ", ";
	os << "sign: " << cs.sign;
	os << ", ";
	os << "delta: " << cs.delta;
	os << ", ";
	os << "ticks: " << cs.ticks;
	os << ", ";
	os << "led_state: " << cs.led_state.state();
	os << ", ";
	os << "button_state: " << cs.button_state;
	os << " }";

	return os;
}

}
