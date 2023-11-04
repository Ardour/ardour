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
#include "lpx.h"

#ifdef LAUNCHPAD_MINI
#define LAUNCHPAD_NAMESPACE LP_MINI
#else
#define LAUNCHPAD_NAMESPACE LP_X
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface::LAUNCHPAD_NAMESPACE;

static ControlProtocol*
new_lpx (Session* s)
{
	LaunchPadX * lpx = nullptr;

	try {
		lpx = new LaunchPadX (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (std::exception & e) {
		error << "Error instantiating LaunchPad X support: " << e.what() << endmsg;
		delete lpx;
		lpx = nullptr;
	}

	return lpx;
}

static void
delete_lpx (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( std::exception & e )
	{
		std::cout << "Exception caught trying to finalize LaunchPad X support: " << e.what() << std::endl;
	}
}

static bool
probe_lpx_midi_protocol ()
{
	std::string i, o;
	return LaunchPadX::probe (i, o);
}


static ControlProtocolDescriptor lpx_descriptor = {
	/* name       */ "Novation LaunchPad X",
	/* id         */ "uri://ardour.org/surfaces/lpx:0",
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ probe_lpx_midi_protocol,
	/* match usb  */ 0, // LaunchPadX::match_usb,
	/* initialize */ new_lpx,
	/* destroy    */ delete_lpx,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &lpx_descriptor; }
