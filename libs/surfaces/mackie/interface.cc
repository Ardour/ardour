/*
	Copyright (C) 2006,2007 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdexcept>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"

#include "control_protocol/control_protocol.h"
#include "mackie_control_protocol.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

static ControlProtocol*
new_mackie_protocol (ControlProtocolDescriptor*, Session* s)
{
	MackieControlProtocol* mcp = 0;
	
	try {
		mcp = new MackieControlProtocol (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (exception & e) {
		error << "Error instantiating MackieControlProtocol: " << e.what() << endmsg;
		delete mcp;
		mcp = 0;
	}
	
	return mcp;
}

static void
delete_mackie_protocol (ControlProtocolDescriptor*, ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to destroy MackieControlProtocol: " << e.what() << endl;
	}
}

/**
	This is called on startup to check whether the lib should be loaded.

	So anything that can be changed in the UI should not be used here to
	prevent loading of the lib.
*/
static bool
probe_mackie_protocol (ControlProtocolDescriptor*)
{
	return MackieControlProtocol::probe();
}

static ControlProtocolDescriptor mackie_descriptor = {
	name : "Mackie",
	id : "uri://ardour.org/surfaces/mackie:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	// actually, the surface does support feedback, but all this
	// flag does is show a submenu on the UI, which is useless for the mackie
	// because feedback is always on. In any case, who'd want to use the
	// mcu without the motorised sliders doing their thing?
	supports_feedback : false,
	probe : probe_mackie_protocol,
	initialize : new_mackie_protocol,
	destroy : delete_mackie_protocol
};
	

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &mackie_descriptor; }
