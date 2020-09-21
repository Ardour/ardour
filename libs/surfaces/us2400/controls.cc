/*
 * Copyright (C) 2006,2007 John Anderson
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <iomanip>
#include <sstream>

#include "ardour/automation_control.h"
#include "pbd/enumwriter.h"

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


using namespace std;
using namespace ArdourSurface;
using namespace US2400;

using ARDOUR::AutomationControl;

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

void
Control::set_control (boost::shared_ptr<AutomationControl> ac)
{
	normal_ac = ac;
}

void
Control::set_value (float val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (normal_ac) {
		normal_ac->set_value (normal_ac->interface_to_internal (val), group_override);
	}
}

float
Control::get_value ()
{
	if (!normal_ac) {
		return 0.0f;
	}
	return normal_ac->internal_to_interface (normal_ac->get_value());
}

void
Control::start_touch (Temporal::timepos_t const & when)
{
	if (normal_ac) {
		return normal_ac->start_touch (when);
	}
}

void
Control::stop_touch (Temporal::timepos_t const & when)
{
	if (normal_ac) {
		return normal_ac->stop_touch (when);
	}
}

ostream & operator <<  (ostream & os, const ArdourSurface::US2400::Control & control)
{
	os << typeid (control).name();
	os << " { ";
	os << "name: " << control.name();
	os << ", ";
	os << "id: " << "0x" << setw(2) << setfill('0') << hex << control.id() << setfill(' ');
	os << ", ";
	os << "group: " << control.group().name();
	os << " }";

	return os;
}
