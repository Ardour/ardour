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

#ifndef _ardour_surface_mcp_http_h_
#define _ardour_surface_mcp_http_h_

#include <stdint.h>
#include <memory>

#include "control_protocol/control_protocol.h"

namespace ARDOUR {
class Session;
}

namespace ArdourSurface {

class MCPHttpServer;

extern const char* const mcp_http_surface_name;
extern const char* const mcp_http_surface_id;

class MCPHttp : public ARDOUR::ControlProtocol
{
public:
	MCPHttp (ARDOUR::Session&);
	virtual ~MCPHttp ();

	int set_active (bool);
	void stripable_selection_changed () {}

	ARDOUR::Session& ardour_session () const;
	uint16_t port () const
	{
		return _port;
	}

private:
	int start ();
	int stop ();

	std::unique_ptr<MCPHttpServer> _server;
	uint16_t _port;
};

} // namespace ArdourSurface

#endif // _ardour_surface_mcp_http_h_
