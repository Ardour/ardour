/*
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/rc_configuration.h"
#include "control_protocol/control_protocol.h"

#include "ardour_websockets.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_ardour_websockets_protocol (ControlProtocolDescriptor* /*descriptor*/,
                                Session* s)
{
	ArdourWebsockets* surface = new ArdourWebsockets (*s);

	surface->set_active (true);

	return surface;
}

static void
delete_ardour_websockets_protocol (ControlProtocolDescriptor* /*descriptor*/,
                                   ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_ardour_websockets_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return true;
}

static void*
ardour_websockets_request_buffer_factory (uint32_t num_requests)
{
	return ArdourWebsockets::request_factory (num_requests);
}

static ControlProtocolDescriptor ardour_websockets_descriptor = {
	/*name :              */ surface_name,
	/*id :                */ surface_id,
	/*ptr :               */ 0,
	/*module :            */ 0,
	/*mandatory :         */ 0,
	/*supports_feedback : */ true,
	/*probe :             */ probe_ardour_websockets_protocol,
	/*initialize :        */ new_ardour_websockets_protocol,
	/*destroy :           */ delete_ardour_websockets_protocol,
	/*request_buffer_factory */ ardour_websockets_request_buffer_factory
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor*
protocol_descriptor ()
{
	return &ardour_websockets_descriptor;
}
