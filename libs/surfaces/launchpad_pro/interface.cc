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
#include "lppro.h"

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

static ControlProtocol*
new_lppro (Session* s)
{
	LaunchPadPro * p2 = 0;

	try {
		p2 = new LaunchPadPro (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (std::exception & e) {
		error << "Error instantiating LaunchPad Pro support: " << e.what() << endmsg;
		delete p2;
		p2 = 0;
	}

	return p2;
}

static void
delete_lppro (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( std::exception & e )
	{
		std::cout << "Exception caught trying to finalize LaunchPad Pro support: " << e.what() << std::endl;
	}
}

static bool
probe_lppro_midi_protocol ()
{
	std::string i, o;
	return LaunchPadPro::probe (i, o);
}


static ControlProtocolDescriptor lppro_descriptor = {
	/* name       */ "Novation LaunchPad Pro",
	/* id         */ "uri://ardour.org/surfaces/lppro:0",
	/* module     */ 0,
	/* available  */ LaunchPadPro::available,
	/* probe_port */ probe_lppro_midi_protocol,
	/* match usb  */ 0, // LaunchPadPro::match_usb,
	/* initialize */ new_lppro,
	/* destroy    */ delete_lppro,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &lppro_descriptor; }
