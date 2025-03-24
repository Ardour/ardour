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
#include "launchkey_4.h"

#ifdef LAUNCHPAD_MINI
#define LAUNCHPAD_NAMESPACE LP_MINI
#else
#define LAUNCHPAD_NAMESPACE LP_X
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface::LAUNCHPAD_NAMESPACE;

static ControlProtocol*
new_lk4 (Session* s)
{
	LaunchKey4 * lk4 = nullptr;

	try {
		lk4 = new LaunchKey4 (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (std::exception & e) {
		error << "Error instantiating LaunchKey 4 support: " << e.what() << endmsg;
		delete lk4;
		lk4 = nullptr;
	}

	return lk4;
}

static void
delete_lk4 (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( std::exception & e )
	{
		std::cout << "Exception caught trying to finalize LaunchKey 4 support: " << e.what() << std::endl;
	}
}

static bool
probe_lk4_midi_protocol ()
{
	std::string i, o;
	return LaunchKey4::probe (i, o);
}


static ControlProtocolDescriptor lk4_descriptor = {
	/* name       */ "Novation LaunchKey 4",
	/* id         */ "uri://ardour.org/surfaces/launchkey4:0",
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ probe_lk4_midi_protocol,
	/* match usb  */ 0, // LaunchKey4::match_usb,
	/* initialize */ new_lk4,
	/* destroy    */ delete_lk4,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &lk4_descriptor; }
