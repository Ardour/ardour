/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_mackie_control_protocol_fader_h__
#define __ardour_mackie_control_protocol_fader_h__

#include "controls.h"

namespace ArdourSurface {

namespace Mackie {

class Fader : public Control
{
  public:

	Fader (int id, std::string name, Group & group)
		: Control (id, name, group)
		, position (0.0)
		, last_update_position (-1)
	{
	}

	MidiByteArray set_position (float);
	MidiByteArray zero() { return set_position (0.0); }

	MidiByteArray update_message ();

	static Control* factory (Surface&, int id, const char*, Group&);

  private:
	float position;
	int   last_update_position;
};

}
}

#endif
