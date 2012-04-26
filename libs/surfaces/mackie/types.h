/*
	Copyright (C) 2006,2007 John Anderson

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
#ifndef mackie_types_h
#define mackie_types_h

#include <iostream>

namespace Mackie
{

enum surface_type_t { 
	mcu, 
	ext, 
};

/**
	This started off as an enum, but it got really annoying
	typing ? on : off
*/
class LedState
{
public:
	enum state_t { none, off, flashing, on };
	LedState()  : _state (none) {}
	LedState (bool yn): _state (yn ? on : off) {}
	LedState (state_t state): _state (state) {}

	LedState& operator= (state_t s) { _state = s; return *this;  }
		
	bool operator ==  (const LedState & other) const
	{
		return state() == other.state();
	}
	
	bool operator !=  (const LedState & other) const
	{
		return state() != other.state();
	}
	
	state_t state() const { return _state; }
	
private:
	state_t _state;
};

extern LedState on;
extern LedState off;
extern LedState flashing;
extern LedState none;

enum ButtonState { neither = -1, release = 0, press = 1 };

/**
	Contains the state for a control, with some convenience
	constructors
*/
struct ControlState
{
	ControlState(): pos(0.0), sign(0), delta(0.0), ticks(0), led_state(off), button_state(neither) {}
	
	ControlState (LedState ls): pos(0.0), delta(0.0), led_state(ls), button_state(neither) {}
	
	// Note that this sets both pos and delta to the flt value
	ControlState (LedState ls, float flt): pos(flt), delta(flt), ticks(0), led_state(ls), button_state(neither) {}
	ControlState (float flt): pos(flt), delta(flt), ticks(0), led_state(none), button_state(neither) {}
	ControlState (float flt, unsigned int tcks): pos(flt), delta(flt), ticks(tcks), led_state(none), button_state(neither) {}
	ControlState (ButtonState bs): pos(0.0), delta(0.0), ticks(0), led_state(none), button_state(bs) {}
	
	/// For faders. Between 0 and 1.
	float pos;
		
	/// For pots. Sign. Either -1 or 1;
	int sign;

	/// For pots. Signed value of total movement. Between 0 and 1
	float delta;
		
	/// For pots. Unsigned number of ticks. Usually between 1 and 16.
	unsigned int ticks;
		
	LedState led_state;
	ButtonState button_state;
};

std::ostream & operator <<  (std::ostream &, const ControlState &);

class Control;
class Fader;
class Button;
class Strip;
class Group;
class Pot;
class Led;

}

#endif
