/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <stdexcept>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"

#include "control_protocol/control_protocol.h"
#include "push2.h"

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

static ControlProtocol*
new_push2 (Session* s)
{
	Push2 * p2 = 0;

	try {
		p2 = new Push2 (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (std::exception & e) {
		error << "Error instantiating Push2 support: " << e.what() << endmsg;
		delete p2;
		p2 = 0;
	}

	return p2;
}

static void
delete_push2 (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( std::exception & e )
	{
		std::cout << "Exception caught trying to finalize Push2 support: " << e.what() << std::endl;
	}
}

static ControlProtocolDescriptor push2_descriptor = {
	/* name       */ "Ableton Push 2",
	/* id         */ "uri://ardour.org/surfaces/push2:0",
	/* module     */ 0,
	/* available  */ Push2::available,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_push2,
	/* destroy    */ delete_push2,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &push2_descriptor; }
