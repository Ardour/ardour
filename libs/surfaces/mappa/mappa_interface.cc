/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <pbd/failed_constructor.h>

#include "control_protocol/control_protocol.h"
#include "oav_mappa.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_mappa_protocol (ControlProtocolDescriptor* /*descriptor*/, Session* s)
{
	Mappa* mp;

	try {
		mp =  new Mappa (*s);
	} catch (failed_constructor& err) {
		return 0;
	}

	if (mp->set_active (true)) {
		delete mp;
		return 0;
	}

	return mp;
}

static void
delete_mappa_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_mappa_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return Mappa::probe ();
}

static void*
mappa_request_buffer_factory (uint32_t num_requests)
{
	return Mappa::request_factory (num_requests);
}

static ControlProtocolDescriptor mappa_descriptor = {
	/*name :              */    "Mappa",
	/*id :                */    "uri://ardour.org/surfaces/mappa:0",
	/*ptr :               */    0,
	/*module :            */    0,
	/*mandatory :         */    0,
	/*supports_feedback : */    true,
	/*probe :             */    probe_mappa_protocol,
	/*initialize :        */    new_mappa_protocol,
	/*destroy :           */    delete_mappa_protocol,
	/*request_buffer_factory */ mappa_request_buffer_factory
};

extern "C" ARDOURSURFACE_API
ControlProtocolDescriptor* protocol_descriptor () {
	return &mappa_descriptor;
}
