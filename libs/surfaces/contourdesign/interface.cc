/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "pbd/failed_constructor.h"
#include "pbd/error.h"

#include "ardour/session.h"
#include "control_protocol/control_protocol.h"

#include "contourdesign.h"

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

static ControlProtocol*
new_contourdesign_protocol (ControlProtocolDescriptor*, Session* s)
{
	ContourDesignControlProtocol* wmcp = new ContourDesignControlProtocol (*s);
	wmcp->set_active (true);
	return wmcp;
}

static void
delete_contourdesign_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_contourdesign_protocol (ControlProtocolDescriptor*)
{
	return ContourDesignControlProtocol::probe ();
}

static void*
contourdesign_request_buffer_factory (uint32_t num_requests)
{
	return ContourDesignControlProtocol::request_factory (num_requests);
}

static ControlProtocolDescriptor contourdesign_descriptor = {
	/* name :                   */ "ContourDesign",
	/* id :                     */ "uri://ardour.org/surfaces/contourdesign:0",
	/* ptr :                    */ 0,
	/* module :                 */ 0,
	/* mandatory :              */ 0,
	/* supports_feedback :      */ false,
	/* probe :                  */ probe_contourdesign_protocol,
	/* initialize :             */ new_contourdesign_protocol,
	/* destroy :                */ delete_contourdesign_protocol,
	/* request_buffer_factory : */ contourdesign_request_buffer_factory
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &contourdesign_descriptor; }
