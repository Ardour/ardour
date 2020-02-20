/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#include "state.h"

NodeState::NodeState ()
{
    update_node_addr_hash ();
}

NodeState::NodeState (std::string node)
    : _node (node)
{
    update_node_addr_hash ();
}

NodeState::NodeState (std::string node, std::initializer_list<uint32_t> addr,
        std::initializer_list<TypedValue> val)
    : _node (node)
    , _addr (addr)
    , _val (val)
{
    update_node_addr_hash ();
}

std::string 
NodeState::debug_str () const
{
    std::stringstream s;
    s << "node = " << _node;

    if (!_addr.empty ()) {
        s << std::endl << " addr = ";

        for (std::vector<uint32_t>::const_iterator it = _addr.begin (); it != _addr.end (); ++it) {
            s << *it << ";";
        }
    }

    for (std::vector<TypedValue>::const_iterator it = _val.begin (); it != _val.end (); ++it) {
        s << std::endl << " val " << it->debug_str ();
    }

    s << std::endl << " hash = " << _node_addr_hash;
    
    return s.str ();
}

int
NodeState::n_addr () const
{
    return static_cast<int>(_addr.size ());
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
    update_node_addr_hash ();
}

int
NodeState::n_val () const
{
    return static_cast<int>(_val.size ());
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

void
NodeState::update_node_addr_hash ()
{
    std::stringstream ss;
    ss << _node;

    for (std::vector<uint32_t>::iterator it = _addr.begin (); it != _addr.end (); ++it) {
        ss << "_" << *it;
    }

    _node_addr_hash = ss.str ();
}
