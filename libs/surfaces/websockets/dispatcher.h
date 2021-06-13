/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#ifndef _ardour_surface_websockets_dispatcher_h_
#define _ardour_surface_websockets_dispatcher_h_

#include <boost/unordered_map.hpp>

#include "client.h"
#include "component.h"
#include "message.h"

namespace ArdourSurface {

class WebsocketsDispatcher : public SurfaceComponent
{
public:
	WebsocketsDispatcher (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~WebsocketsDispatcher (){};

	void dispatch (Client, const NodeStateMessage&);
	void update_all_nodes (Client);

private:
	typedef void (WebsocketsDispatcher::*DispatcherMethod) (Client, const NodeStateMessage&);
	typedef boost::unordered_map<std::string, DispatcherMethod> NodeMethodMap;

	static NodeMethodMap _node_to_method;

	void transport_tempo_handler (Client, const NodeStateMessage&);
	void transport_roll_handler (Client client, const NodeStateMessage&);
	void transport_record_handler (Client client, const NodeStateMessage&);
	void strip_gain_handler (Client, const NodeStateMessage&);
	void strip_pan_handler (Client, const NodeStateMessage&);
	void strip_mute_handler (Client, const NodeStateMessage&);
	void strip_plugin_enable_handler (Client, const NodeStateMessage&);
	void strip_plugin_param_value_handler (Client, const NodeStateMessage&);

	void update (Client, std::string, TypedValue);
	void update (Client, std::string, uint32_t, TypedValue);
	void update (Client, std::string, uint32_t, uint32_t, TypedValue);
	void update (Client, std::string, uint32_t, uint32_t, uint32_t, TypedValue);
	void update (Client, std::string, const AddressVector&, const ValueVector&);
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_dispatcher_h_
