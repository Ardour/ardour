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

#include <iostream>
#include <iomanip>
#include <sstream>

#include "controls.h"
#include "types.h"
#include "surface.h"
#include "control_group.h"

#include "button.h"
#include "led.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"


using namespace Mackie;
using namespace std;

void Group::add (Control& control)
{
	_controls.push_back (&control);
}

Control::Control (int id, std::string name, Group & group)
	: _id (id)
	, _name (name)
	, _group (group)
	, _in_use (false)
{
}

/** @return true if the control is in use, or false otherwise.
    Buttons are `in use' when they are held down.
    Faders with touch support are `in use' when they are being touched.
    Pots, or faders without touch support, are `in use' from the first move
    event until a timeout after the last move event.
*/
bool
Control::in_use () const
{
	return _in_use;
}

void
Control::set_in_use (bool in_use)
{
	_in_use = in_use;
}

ostream & Mackie::operator <<  (ostream & os, const Mackie::Control & control)
{
	os << typeid (control).name();
	os << " { ";
	os << "name: " << control.name();
	os << ", ";
	os << "raw_id: " << "0x" << setw(2) << setfill('0') << hex << control.raw_id() << setfill(' ');
	os << ", ";
	os << "group: " << control.group().name();
	os << " }";
	
	return os;
}

Control*
Jog::factory (Surface& surface, int id, const char* name, Group& group)
{
	Jog* j = new Jog (id, name, group);
	surface.controls.push_back (j);
	surface.controls_by_name["jog"] = j;
	group.add (*j);
	return j;
}

