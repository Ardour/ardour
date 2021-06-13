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

#ifndef _ardour_surface_websockets_message_h_
#define _ardour_surface_websockets_message_h_

#include "state.h"

namespace ArdourSurface {

class NodeStateMessage
{
public:
	NodeStateMessage (const NodeState& state);
	NodeStateMessage (void*, size_t);

	size_t serialize (void*, size_t) const;

	bool is_valid () const
	{
		return _valid;
	}
	bool is_write () const
	{
		return _write;
	}
	const NodeState& state () const
	{
		return _state;
	}

private:
	bool      _valid;
	bool      _write;
	NodeState _state;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_message_h_
