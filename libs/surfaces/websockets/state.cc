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

#include <boost/unordered_set.hpp>
#include <sstream>

#include "state.h"

using namespace ArdourSurface;

NodeState::NodeState () {}

NodeState::NodeState (std::string node)
    : _node (node)
{
}

NodeState::NodeState (std::string node, AddressVector addr, ValueVector val)
    : _node (node)
    , _addr (addr)
    , _val (val)
{
}

std::string
NodeState::debug_str () const
{
	std::stringstream s;
	s << "node = " << _node;

	if (!_addr.empty ()) {
		s << std::endl
		  << " addr = ";

		for (AddressVector::const_iterator it = _addr.begin (); it != _addr.end (); ++it) {
			s << *it << ";";
		}
	}

	for (ValueVector::const_iterator it = _val.begin (); it != _val.end (); ++it) {
		s << std::endl
		  << " val " << it->debug_str ();
	}

	return s.str ();
}

int
NodeState::n_addr () const
{
	return static_cast<int> (_addr.size ());
}

uint32_t
NodeState::nth_addr (int n) const
{
	return _addr[n];
}

void
NodeState::add_addr (uint32_t addr)
{
	_addr.push_back (addr);
}

int
NodeState::n_val () const
{
	return static_cast<int> (_val.size ());
}

TypedValue
NodeState::nth_val (int n) const
{
	if (n_val () < n) {
		return TypedValue ();
	}

	return _val[n];
}

void
NodeState::add_val (TypedValue val)
{
	_val.push_back (val);
}

std::size_t
NodeState::node_addr_hash () const
{
	std::size_t seed = 0;
	boost::hash_combine (seed, _node);
	boost::hash_combine (seed, _addr);
	return seed;
}

bool
NodeState::operator== (const NodeState& other) const
{
	return node_addr_hash () == other.node_addr_hash ();
}

bool
NodeState::operator< (const NodeState& other) const
{
	return node_addr_hash () < other.node_addr_hash ();
}


std::size_t
hash_value (const NodeState& state)
{
	return state.node_addr_hash ();
}
