/*
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

#ifndef __ardour_us2400_control_protocol_button_h__
#define __ardour_us2400_control_protocol_button_h__

#include "ardour/types.h"

#include "controls.h"
#include "led.h"

namespace ArdourSurface {

namespace US2400 {

class Surface;

class Button : public Control
{
public:
/* These values uniquely identify each possible button that an MCP device may
   send. Each DeviceInfo object contains its own set of button definitions that
   define what device ID will be sent for each button, and there is no reason
   for them to be the same.  */

	enum ID {
		/* Global Buttons */

		Scrub,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		Rewind,
		Ffwd,
		Stop,
		Play,
		Record,
		Left,
		Right,
		Flip,
		MstrSelect,

		FinalGlobalButton,


		/* Global buttons that users should not redefine */

		Drop,
		Send,
		Pan,
		ClearSolo,
		Shift,
		Option,
		Ctrl,
		CmdAlt,

		/* Strip buttons */

		Solo,
		Mute,
		Select,
		FaderTouch,

		/* Master fader */

		MasterFaderTouch,
	};


	Button (Surface& s, ID bid, int did, std::string name, Group & group)
		: Control (did, name, group)
		, _surface (s)
		, _bid (bid)
		, _led  (did, name + "_led", group)
		, press_time (0) {}

	MidiByteArray zero() { return _led.zero (); }
	MidiByteArray set_state (LedState ls) { return _led.set_state (ls); }

	ID bid() const { return _bid; }

	static Control* factory (Surface& surface, Button::ID bid, int id, const std::string&, Group& group);
	static int name_to_id (const std::string& name);
	static std::string id_to_name (Button::ID);

	Surface& surface() const { return _surface; }

	void mark_dirty() { _led.mark_dirty(); }

	void pressed ();
	void released ();

	int32_t long_press_count ();

private:
	Surface& _surface;
	ID  _bid; /* device independent button ID */
	Led _led;
	PBD::microseconds_t press_time;
};

} // US2400 namespace
} // ArdourSurface namespace

#endif
