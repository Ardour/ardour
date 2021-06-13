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

#include <sstream>

#include "client.h"

using namespace ArdourSurface;

bool
ClientContext::has_state (const NodeState& node_state)
{
	ClientState::iterator it = _state.find (node_state);

	if (it == _state.end ()) {
		return false;
	}

	int n_val = node_state.n_val ();

	if (it->n_val () != n_val) {
		return false;
	}

	for (int i = 0; i < n_val; i++) {
		if (it->nth_val (i) != node_state.nth_val (i)) {
			return false;
		}
	}

	return true;
}

void
ClientContext::update_state (const NodeState& node_state)
{
	ClientState::iterator it = _state.find (node_state);

	if (it != _state.end ()) {
		_state.erase (it);
	}

	_state.insert (node_state);
}

std::string
ClientContext::debug_str ()
{
	std::stringstream ss;

	ss << "client = " << std::hex << _wsi << std::endl;

	for (ClientState::iterator it = _state.begin (); it != _state.end (); ++it) {
		ss << " - " << it->debug_str () << std::endl;
	}

	return ss.str ();
}
