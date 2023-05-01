/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
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
#include "launch_control_xl.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace ArdourSurface;

static ControlProtocol*
new_launch_control_xl (Session* s)
{
	LaunchControlXL * lcxl = 0;

	try {
		lcxl = new LaunchControlXL (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (exception & e) {
		error << "Error instantiating LaunchControlXL support: " << e.what() << endmsg;
		delete lcxl;
		lcxl = 0;
	}

	return lcxl;
}

static void
delete_launch_control_xl (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to finalize LaunchControlXL support: " << e.what() << endl;
	}
}

static ControlProtocolDescriptor launch_control_xl_descriptor = {
	/* name       */ "Novation Launch Control XL",
	/* id         */ "uri://ardour.org/surfaces/launch_control_xl:0",
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_launch_control_xl,
	/* destroy    */ delete_launch_control_xl,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &launch_control_xl_descriptor; }
