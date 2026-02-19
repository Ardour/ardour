/*
 * Copyright (C) 2026
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

#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/session.h"

#include "mcp_http.h"
#include "mcp_http_server.h"

using namespace ARDOUR;
using namespace ArdourSurface;

const char* const ArdourSurface::mcp_http_surface_name = "MCP HTTP Server (Experimental)";
const char* const ArdourSurface::mcp_http_surface_id = "uri://ardour.org/surfaces/mcp_http:0";

MCPHttp::MCPHttp (Session& s)
	: ControlProtocol (s, X_(mcp_http_surface_name))
	, _port (4820)
{
}

MCPHttp::~MCPHttp ()
{
	stop ();
}

int
MCPHttp::set_active (bool yn)
{
	if (yn != active ()) {
		if (yn) {
			if (start ()) {
				return -1;
			}
		} else {
			if (stop ()) {
				return -1;
			}
		}
	}

	return ControlProtocol::set_active (yn);
}

Session&
MCPHttp::ardour_session () const
{
	return *session;
}

int
MCPHttp::start ()
{
	if (_server) {
		return 0;
	}

	_server.reset (new MCPHttpServer (*session, _port));
	if (_server->start ()) {
		_server.reset ();
		PBD::error << "MCPHttp: failed to start server" << endmsg;
		return -1;
	}

	PBD::info << "MCPHttp: started on http://127.0.0.1:" << _port << endmsg;

	return 0;
}

int
MCPHttp::stop ()
{
	if (!_server) {
		return 0;
	}

	_server->stop ();
	_server.reset ();

	PBD::info << "MCPHttp: stopped" << endmsg;

	return 0;
}
