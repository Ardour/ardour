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

#ifndef node_state_h
#define node_state_h

#include <vector>
#include <cmath>
#include <cstring>

#include "typed_value.h"

namespace Node {
    const std::string tempo                     = "tempo";
    const std::string strip_desc                = "strip_desc";
    const std::string strip_meter               = "strip_meter";
    const std::string strip_gain                = "strip_gain";
    const std::string strip_pan                 = "strip_pan";
    const std::string strip_mute                = "strip_mute";
    const std::string strip_plugin_desc         = "strip_plugin_desc";
    const std::string strip_plugin_enable       = "strip_plugin_enable";
    const std::string strip_plugin_param_desc   = "strip_plugin_param_desc";
    const std::string strip_plugin_param_value  = "strip_plugin_param_value";
};

class NodeState {

  public:

    NodeState ();
    NodeState (std::string);
    NodeState (std::string, std::initializer_list<uint32_t>,
        std::initializer_list<TypedValue> = {});

    std::string debug_str () const;

    std::string node () const { return _node; }

    int n_addr () const;
    uint32_t nth_addr (int) const;
    void add_addr (uint32_t);

    int n_val () const;
    TypedValue nth_val (int) const;
    void add_val (TypedValue);

  private:

    std::string _node;
    std::vector<uint32_t> _addr;
    std::vector<TypedValue> _val;
    std::string _node_addr_hash;

    void update_node_addr_hash ();

    friend struct std::hash<NodeState>;
    friend struct std::equal_to<NodeState>;

};

namespace std {
    template <>
    struct hash<NodeState> {
        size_t operator () (const NodeState &state) const {
            // std::hash<const char*> produces a hash of the value of the
            // pointer (the memory address), it does not examine the contents
            // of any character array.
            return std::hash<std::string>()(state._node_addr_hash);
        }
    };

    template<>
    struct equal_to<NodeState> {
        bool operator() (const NodeState& lhs, const NodeState& rhs) const {
            return lhs._node_addr_hash == rhs._node_addr_hash;
        }
    };
}

#endif // node_state_h
