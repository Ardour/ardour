/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

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
