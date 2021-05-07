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

#include <cmath>

#include "pbd/compose.h"

#include "ardour/debug.h"

#include "fader.h"
#include "surface.h"
#include "control_group.h"
#include "mackie_control_protocol.h"

using namespace ArdourSurface;
using namespace Mackie;
using namespace PBD;

Control*
Fader::factory (Surface& surface, int id, const char* name, Group& group)
{
	Fader* f = new Fader (id, name, group);

	surface.faders[id] = f;
	surface.controls.push_back (f);
	group.add (*f);
	return f;
}

MidiByteArray
Fader::set_position (float normalized)
{
	position = normalized;
	return update_message ();
}

MidiByteArray
Fader::update_message ()
{
	MackieControlProtocol* mcp = MackieControlProtocol::instance ();
	if (mcp && mcp->flip_mode() == MackieControlProtocol::Zero) {
		/* do not send messages to move the faders when in this mode */
		return MidiByteArray();
	}

	int posi = lrintf (16383.0 * position);

	if (posi == last_update_position) {
		return MidiByteArray();
	}

	last_update_position = posi;

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("generate fader message for position %1 (%2)\n", position, posi));
	return MidiByteArray  (3, 0xe0 + id(), posi & 0x7f, posi >> 7);
}
