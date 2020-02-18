/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 W.P. van Paass
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

#include <pbd/failed_constructor.h>

#include "control_protocol/control_protocol.h"
#include "cc121.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_cc121_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, Session* s)
{
	CC121* fp;

	try {
		fp =  new CC121 (*s);
	} catch (failed_constructor& err) {
		return 0;
	}

	if (fp->set_active (true)) {
		delete fp;
		return 0;
	}

	return fp;
}

static void
delete_cc121_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_cc121_midi_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return CC121::probe ();
}

static void*
cc121_request_buffer_factory (uint32_t num_requests)
{
	return CC121::request_factory (num_requests);
}

static ControlProtocolDescriptor cc121_midi_descriptor = {
	/*name :              */   "Steinberg CC121",
	/*id :                */   "uri://ardour.org/surfaces/cc121:0",
	/*ptr :               */   0,
	/*module :            */   0,
	/*mandatory :         */   0,
	/*supports_feedback : */   true,
	/*probe :             */   probe_cc121_midi_protocol,
	/*initialize :        */   new_cc121_midi_protocol,
	/*destroy :           */   delete_cc121_midi_protocol,
	/*request_buffer_factory */ cc121_request_buffer_factory
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &cc121_midi_descriptor; }

