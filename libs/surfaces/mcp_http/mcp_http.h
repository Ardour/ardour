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
#include <string>

#include "control_protocol/control_protocol.h"
#include "pbd/event_loop.h"

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
	enum DebugLevel {
		DebugOff = 0,
		DebugBasic = 1,
		DebugVerbose = 2
	};

	MCPHttp (ARDOUR::Session&);
	virtual ~MCPHttp ();

	XMLNode& get_state () const override;
	int set_state (const XMLNode&, int version) override;

	bool has_editor () const override { return true; }
	void* get_gui () const override;
	void tear_down_gui () override;

	int set_active (bool) override;
	void stripable_selection_changed () override {}

	ARDOUR::Session& ardour_session () const;

	void set_port (uint16_t);
	uint16_t port () const
	{
		return _port;
	}

	void set_debug_level (int);
	int debug_level () const
	{
		return _debug_level;
	}

	std::string endpoint_url () const;
	std::string protocol_name () const;

private:
	int start ();
	int stop ();
	void build_gui ();

	std::unique_ptr<MCPHttpServer> _server;
	uint16_t _port;
	int _debug_level;
	PBD::EventLoop* _event_loop;
	mutable void* _gui;
};

} // namespace ArdourSurface

#endif // _ardour_surface_mcp_http_h_
