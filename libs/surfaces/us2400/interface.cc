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

#include <stdexcept>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"

#include "control_protocol/control_protocol.h"
#include "us2400_control_protocol.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace ArdourSurface;
using namespace US2400;

static ControlProtocol*
new_us2400_protocol (Session* s)
{
	US2400Protocol* mcp = 0;

	try {
		mcp = new US2400Protocol (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (exception & e) {
		error << "Error instantiating US-2400: " << e.what() << endmsg;
		delete mcp;
		mcp = 0;
	}

	return mcp;
}

static void
delete_us2400_protocol (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to destroy US-2400: " << e.what() << endl;
	}
}

static ControlProtocolDescriptor us2400_descriptor = {
	/* name       */ "Tascam US-2400",
	/* id         */ "uri://ardour.org/surfaces/us2400:0",
	/* module     */ 0,
	/* avilable   */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_us2400_protocol,
	/* destroy    */ delete_us2400_protocol,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &us2400_descriptor; }
