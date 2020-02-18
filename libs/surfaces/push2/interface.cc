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
using namespace std;
using namespace ArdourSurface;

static ControlProtocol*
new_push2 (ControlProtocolDescriptor*, Session* s)
{
	Push2 * p2 = 0;

	try {
		p2 = new Push2 (*s);
		/* do not set active here - wait for set_state() */
	}
	catch (exception & e) {
		error << "Error instantiating Push2 support: " << e.what() << endmsg;
		delete p2;
		p2 = 0;
	}

	return p2;
}

static void
delete_push2 (ControlProtocolDescriptor*, ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to finalize Push2 support: " << e.what() << endl;
	}
}

/**
	This is called on startup to check whether the lib should be loaded.

	So anything that can be changed in the UI should not be used here to
	prevent loading of the lib.
*/
static bool
probe_push2 (ControlProtocolDescriptor*)
{
	return Push2::probe();
}

static void*
push2_request_buffer_factory (uint32_t num_requests)
{
	return Push2::request_factory (num_requests);
}

static ControlProtocolDescriptor push2_descriptor = {
	/*name :              */   "Ableton Push 2",
	/*id :                */   "uri://ardour.org/surfaces/push2:0",
	/*ptr :               */   0,
	/*module :            */   0,
	/*mandatory :         */   0,
	// actually, the surface does support feedback, but all this
	// flag does is show a submenu on the UI, which is useless for the mackie
	// because feedback is always on. In any case, who'd want to use the
	// mcu without the motorised sliders doing their thing?
	/*supports_feedback : */   false,
	/*probe :             */   probe_push2,
	/*initialize :        */   new_push2,
	/*destroy :           */   delete_push2,
	/*request_buffer_factory */ push2_request_buffer_factory
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &push2_descriptor; }
