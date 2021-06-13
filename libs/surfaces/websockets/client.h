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

#ifndef _ardour_surface_websockets_client_h_
#define _ardour_surface_websockets_client_h_

#include <set>
#include <list>

#include "message.h"
#include "state.h"

typedef struct lws* Client;

namespace ArdourSurface {

typedef std::list<NodeStateMessage> ClientOutputBuffer;

class ClientContext
{
public:
	ClientContext (Client wsi)
	    : _wsi (wsi){};
	virtual ~ClientContext (){};

	Client wsi () const
	{
		return _wsi;
	}

	bool has_state (const NodeState&);
	void update_state (const NodeState&);

	ClientOutputBuffer& output_buf ()
	{
		return _output_buf;
	}

	std::string debug_str ();

private:
	Client _wsi;

	typedef std::set<NodeState> ClientState;
	ClientState                 _state;

	ClientOutputBuffer _output_buf;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_client_h_
