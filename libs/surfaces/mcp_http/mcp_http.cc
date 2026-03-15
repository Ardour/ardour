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

#include "pbd/error.h"
#include "pbd/event_loop.h"
#include "pbd/i18n.h"
#include "pbd/xml++.h"

#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "mcp_http.h"
#include "mcp_http_gui.h"
#include "mcp_http_server.h"

using namespace ARDOUR;
using namespace ArdourSurface;

namespace {

template <typename T>
static bool
read_global_protocol_property (const char* key, T& value)
{
	if (!ARDOUR::Config) {
		return false;
	}

	XMLNode* cps = ARDOUR::Config->control_protocol_state ();
	if (!cps) {
		return false;
	}

	const XMLNodeList children = cps->children ();
	for (XMLNodeConstIterator it = children.begin (); it != children.end (); ++it) {
		const XMLNode* child = *it;
		if (child->name () != X_("Protocol")) {
			continue;
		}

		std::string name;
		if (!child->get_property (X_("name"), name) || name != ArdourSurface::mcp_http_surface_name) {
			continue;
		}

		return child->get_property (X_(key), value);
	}

	return false;
}

} // namespace

const char* const ArdourSurface::mcp_http_surface_name = "MCP HTTP Server (Experimental)";
const char* const ArdourSurface::mcp_http_surface_id = "uri://ardour.org/surfaces/mcp_http:0";

MCPHttp::MCPHttp (Session& s)
	: ControlProtocol (s, X_(mcp_http_surface_name))
	, _port (4820)
	, _debug_level (DebugOff)
	, _event_loop (PBD::EventLoop::get_event_loop_for_thread ())
	, _gui (0)
{
}

MCPHttp::~MCPHttp ()
{
	tear_down_gui ();
	stop ();
}

XMLNode&
MCPHttp::get_state () const
{
	XMLNode& node (ControlProtocol::get_state ());
	node.set_property (X_("port"), (uint32_t) _port);
	node.set_property (X_("debug-level"), (int32_t) _debug_level);
	return node;
}

int
MCPHttp::set_state (const XMLNode& node, int version)
{
	if (node.name () == X_("Protocol") && ControlProtocol::set_state (node, version)) {
		return -1;
	}

	uint32_t port = _port;
	if (!node.get_property (X_("port"), port)) {
		read_global_protocol_property ("port", port);
	}
	if (port >= 1 && port <= 65535) {
		set_port ((uint16_t) port);
	}

	int32_t debug_level = _debug_level;
	if (!node.get_property (X_("debug-level"), debug_level)) {
		read_global_protocol_property ("debug-level", debug_level);
	}
	set_debug_level ((int) debug_level);

	return 0;
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

void
MCPHttp::set_port (uint16_t port)
{
	if (port == _port || port == 0) {
		return;
	}

	_port = port;

	if (active ()) {
		stop ();
		if (start ()) {
			ControlProtocol::set_active (false);
		}
	}
}

void
MCPHttp::set_debug_level (int level)
{
	if (level < (int) DebugOff) {
		level = DebugOff;
	} else if (level > (int) DebugVerbose) {
		level = DebugVerbose;
	}

	_debug_level = level;
	if (_server) {
		_server->set_debug_level (_debug_level);
	}
}

std::string
MCPHttp::endpoint_url () const
{
	return std::string ("http://127.0.0.1:") + std::to_string (_port) + "/mcp";
}

std::string
MCPHttp::protocol_name () const
{
	return "Streamable HTTP";
}

int
MCPHttp::start ()
{
	if (_server) {
		return 0;
	}

	_server.reset (new MCPHttpServer (*session, _port, _debug_level, _event_loop));
	if (_server->start ()) {
		_server.reset ();
		PBD::error << "MCPHttp: failed to start server" << endmsg;
		return -1;
	}

	PBD::info << "MCPHttp: started on " << endpoint_url ()
		<< " (" << protocol_name () << ")" << endmsg;

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
