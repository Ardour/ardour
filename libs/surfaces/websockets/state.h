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

#ifndef _ardour_surface_websockets_state_h_
#define _ardour_surface_websockets_state_h_

#include <climits>
#include <cmath>
#include <cstring>
#include <stdint.h>
#include <vector>

#include "typed_value.h"

#define ADDR_NONE UINT_MAX

namespace ArdourSurface {

namespace Node
{
	const std::string strip_description              = "strip_description";
	const std::string strip_meter                    = "strip_meter";
	const std::string strip_gain                     = "strip_gain";
	const std::string strip_pan                      = "strip_pan";
	const std::string strip_mute                     = "strip_mute";
	const std::string strip_plugin_description       = "strip_plugin_description";
	const std::string strip_plugin_enable            = "strip_plugin_enable";
	const std::string strip_plugin_param_description = "strip_plugin_param_description";
	const std::string strip_plugin_param_value       = "strip_plugin_param_value";
	const std::string transport_tempo                = "transport_tempo";
	const std::string transport_time                 = "transport_time";
	const std::string transport_roll                 = "transport_roll";
	const std::string transport_record               = "transport_record";
} // namespace Node

typedef std::vector<uint32_t>   AddressVector;
typedef std::vector<TypedValue> ValueVector;

class NodeState
{
public:
	NodeState ();
	NodeState (std::string);
	NodeState (std::string, AddressVector, ValueVector = ValueVector ());

	std::string debug_str () const;

	std::string node () const
	{
		return _node;
	}

	int      n_addr () const;
	uint32_t nth_addr (int) const;
	void     add_addr (uint32_t);

	int        n_val () const;
	TypedValue nth_val (int) const;
	void       add_val (TypedValue);

	std::size_t node_addr_hash () const;

	bool operator== (const NodeState& other) const;
	bool operator< (const NodeState& other) const;

private:
	std::string   _node;
	AddressVector _addr;
	ValueVector   _val;
};

std::size_t
hash_value (const NodeState&);

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_state_h_
