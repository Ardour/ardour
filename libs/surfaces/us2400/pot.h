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

#ifndef __ardour_us2400_control_protocol_pot_h__
#define __ardour_us2400_control_protocol_pot_h__

#include "controls.h"

namespace ArdourSurface {

namespace US2400 {

class Pot : public Control
{
public:
	static int const External;
	static int const ID;

	enum Mode {
		dot = 0,
		boost_cut = 1,
		wrap = 2,
		spread = 3
	};

	Pot (int id, std::string name, Group & group)
		: Control (id, name, group),
		last_update_position (-1),
		llast_update_position (-1) {}
		
	void set_mode(Mode m) {_mode = m; last_update_position = -1; }

	MidiByteArray set (float, bool);
	MidiByteArray zero() { return set (0.0, false); }

	static Control* factory (Surface&, int id, const char*, Group&);

	void mark_dirty() { last_update_position = llast_update_position = -1; }

	int   last_update_position;
	int   llast_update_position;
	
	Mode _mode;

};

}
}

#endif /* __ardour_us2400_control_protocol_pot_h__ */
