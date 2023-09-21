/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#include <stdexcept>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"
#include "ardour/debug.h"

#include "control_protocol/control_protocol.h"

#include "mackie_control_protocol.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace ArdourSurface;
using namespace ArdourSurface::MACKIE_NAMESPACE;

#define PROTOCOL_NAME ("SSL 360: UF8 UF1")

static ControlProtocol*
new_uf8_protocol (Session* s)
{
	MackieControlProtocol* mcp = 0;

	DEBUG_TRACE (DEBUG::MackieControl, "making uf8-protocol");

	try {
		mcp = new MackieControlProtocol (*s, PROTOCOL_NAME);
		/* do not set active here - wait for set_state() */
	}
	catch (exception & e) {
		error << "Error instantiating MackieControlProtocol for UF8: " << e.what() << endmsg;
		delete mcp;
		mcp = 0;
	}

	return mcp;
}

static void
delete_uf8_protocol (ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to destroy MackieControlProtocol or UF8: " << e.what() << endl;
	}
}

// Field names commented out by JE - 06-01-2010
static ControlProtocolDescriptor uf8_descriptor = {
	/* name       */ PROTOCOL_NAME,
	/* id         */ "uri://ardour.org/surfaces/ssl_uf8:0",
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_uf8_protocol,
	/* destroy    */ delete_uf8_protocol,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &uf8_descriptor; }
