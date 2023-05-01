/*
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#include "ardour/rc_configuration.h"
#include "control_protocol/control_protocol.h"
#include "osc.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_osc_protocol (Session* s)
{
	OSC* osc = new OSC (*s, Config->get_osc_port());

	osc->set_active (true);

	return osc;
}

static void
delete_osc_protocol (ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor osc_descriptor = {
	/* name       */ "Open Sound Control (OSC)",
	/* id         */ "uri://ardour.org/surfaces/osc:0",
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_osc_protocol,
	/* destroy    */ delete_osc_protocol,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &osc_descriptor; }

