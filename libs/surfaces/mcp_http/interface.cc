/*
 * Copyright (C) 2026 Frank Povazanj <frank.povazanj@gmail.com>
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

#include "control_protocol/control_protocol.h"

#include "mcp_http.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_mcp_http_protocol (Session* s)
{
	MCPHttp* surface = new MCPHttp (*s);

	surface->set_active (true);

	return surface;
}

static void
delete_mcp_http_protocol (ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor mcp_http_descriptor = {
	/* name       */ mcp_http_surface_name,
	/* id         */ mcp_http_surface_id,
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_mcp_http_protocol,
	/* destroy    */ delete_mcp_http_protocol,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor*
protocol_descriptor ()
{
	return &mcp_http_descriptor;
}

