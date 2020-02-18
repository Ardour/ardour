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

#include "control_protocol/control_protocol.h"
#include "mackie_control_protocol.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace ArdourSurface;
using namespace Mackie;

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

static void*
mackie_request_buffer_factory (uint32_t num_requests)
{
	return MackieControlProtocol::request_factory (num_requests);
}

// Field names commented out by JE - 06-01-2010
static ControlProtocolDescriptor mackie_descriptor = {
	/*name :              */   "Mackie",
	/*id :                */   "uri://ardour.org/surfaces/mackie:0",
	/*ptr :               */   0,
	/*module :            */   0,
	/*mandatory :         */   0,
	// actually, the surface does support feedback, but all this
	// flag does is show a submenu on the UI, which is useless for the mackie
	// because feedback is always on. In any case, who'd want to use the
	// mcu without the motorised sliders doing their thing?
	/*supports_feedback : */   false,
	/*probe :             */   probe_mackie_protocol,
	/*initialize :        */   new_mackie_protocol,
	/*destroy :           */   delete_mackie_protocol,
	/*request_buffer_factory */ mackie_request_buffer_factory
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &mackie_descriptor; }
