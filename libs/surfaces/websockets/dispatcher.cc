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

#include <boost/assign.hpp>

#include "ardour/plugin_insert.h"

#include "ardour_websockets.h"
#include "dispatcher.h"
#include "state.h"

using namespace ARDOUR;

#define NODE_METHOD_PAIR(x) (Node::x, &WebsocketsDispatcher::x##_handler)

WebsocketsDispatcher::NodeMethodMap
	WebsocketsDispatcher::_node_to_method = boost::assign::map_list_of
		NODE_METHOD_PAIR (tempo)
		NODE_METHOD_PAIR (transport_roll)
		NODE_METHOD_PAIR (record_state)
		NODE_METHOD_PAIR (strip_gain)
		NODE_METHOD_PAIR (strip_pan)
		NODE_METHOD_PAIR (strip_mute)
		NODE_METHOD_PAIR (strip_plugin_enable)
		NODE_METHOD_PAIR (strip_plugin_param_value);

void
WebsocketsDispatcher::dispatch (Client client, const NodeStateMessage& msg)
{
	NodeMethodMap::iterator it = _node_to_method.find (msg.state ().node ());
	if (it != _node_to_method.end ()) {
		try {
			(this->*it->second) (client, msg);
		} catch (const std::exception& e) {
			std::cerr << e.what () << std::endl;
		}
	}
}

void
WebsocketsDispatcher::update_all_nodes (Client client)
{
	update (client, Node::tempo, globals ().tempo ());
	update (client, Node::position_time, globals ().position_time ());
	update (client, Node::transport_roll, globals ().transport_roll ());
	update (client, Node::record_state, globals ().record_state ());

	for (uint32_t strip_n = 0; strip_n < strips ().strip_count (); ++strip_n) {
		boost::shared_ptr<Stripable> strip = strips ().nth_strip (strip_n);

		bool is_vca = strip->presentation_info ().flags () & ARDOUR::PresentationInfo::VCA;

		AddressVector strip_addr = AddressVector ();
		strip_addr.push_back (strip_n);
		ValueVector strip_desc = ValueVector ();
		strip_desc.push_back (strip->name ());
		strip_desc.push_back (is_vca);
		
		update (client, Node::strip_description, strip_addr, strip_desc);
		
		update (client, Node::strip_gain, strip_n, strips ().strip_gain (strip_n));
		update (client, Node::strip_mute, strip_n, strips ().strip_mute (strip_n));

		// Pan and plugins not available in VCAs
		if (is_vca) {
			continue;
		}

		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (strip);
		if (!route) {
			continue;
		}

		update (client, Node::strip_pan, strip_n, strips ().strip_pan (strip_n));

		for (uint32_t plugin_n = 0;; ++plugin_n) {
			boost::shared_ptr<PluginInsert> insert = strips ()
			                                             .strip_plugin_insert (strip_n, plugin_n);
			if (!insert) {
				break;
			}

			boost::shared_ptr<Plugin> plugin = insert->plugin ();
			update (client, Node::strip_plugin_description, strip_n, plugin_n,
			        static_cast<std::string> (plugin->name ()));

			update (client, Node::strip_plugin_enable, strip_n, plugin_n,
			        strips ().strip_plugin_enabled (strip_n, plugin_n));

			for (uint32_t param_n = 0; param_n < plugin->parameter_count (); ++param_n) {
				boost::shared_ptr<AutomationControl> a_ctrl =
				    strips ().strip_plugin_param_control (strip_n, plugin_n, param_n);
				if (!a_ctrl) {
					continue;
				}

				AddressVector addr = AddressVector ();
				addr.push_back (strip_n);
				addr.push_back (plugin_n);
				addr.push_back (param_n);

				ValueVector val = ValueVector ();
				val.push_back (a_ctrl->name ());

				// possible flags: enumeration, integer_step, logarithmic, sr_dependent, toggled
				ParameterDescriptor pd = a_ctrl->desc ();

				if (pd.toggled) {
					val.push_back (std::string ("b"));
				} else if (pd.enumeration || pd.integer_step) {
					val.push_back (std::string ("i"));
					val.push_back (pd.lower);
					val.push_back (pd.upper);
				} else {
					val.push_back (std::string ("d"));
					val.push_back (pd.lower);
					val.push_back (pd.upper);
					val.push_back (pd.logarithmic);
				}

				update (client, Node::strip_plugin_param_description, addr, val);

				TypedValue value = strips ().strip_plugin_param_value (strip_n, plugin_n, param_n);
				update (client, Node::strip_plugin_param_value, strip_n, plugin_n, param_n, value);
			}
		}
	}
}

void
WebsocketsDispatcher::tempo_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		globals ().set_tempo (state.nth_val (0));
	} else {
		update (client, Node::tempo, globals ().tempo ());
	}
}

void
WebsocketsDispatcher::transport_roll_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		globals ().set_transport_roll (state.nth_val (0));
	} else {
		update (client, Node::transport_roll, globals ().transport_roll ());
	}
}

void
WebsocketsDispatcher::record_state_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		globals ().set_record_state (state.nth_val (0));
	} else {
		update (client, Node::record_state, globals ().record_state ());
	}
}

void
WebsocketsDispatcher::strip_gain_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (state.n_addr () < 1) {
		return;
	}

	uint32_t strip_id = state.nth_addr (0);

	if (msg.is_write () && (state.n_val () > 0)) {
		strips ().set_strip_gain (strip_id, state.nth_val (0));
	} else {
		update (client, Node::strip_gain, strip_id, strips ().strip_gain (strip_id));
	}
}

void
WebsocketsDispatcher::strip_pan_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (state.n_addr () < 1) {
		return;
	}

	uint32_t strip_id = state.nth_addr (0);

	if (msg.is_write () && (state.n_val () > 0)) {
		strips ().set_strip_pan (strip_id, state.nth_val (0));
	} else {
		update (client, Node::strip_pan, strip_id, strips ().strip_pan (strip_id));
	}
}

void
WebsocketsDispatcher::strip_mute_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (state.n_addr () < 1) {
		return;
	}

	uint32_t strip_id = state.nth_addr (0);

	if (msg.is_write () && (state.n_val () > 0)) {
		strips ().set_strip_mute (strip_id, state.nth_val (0));
	} else {
		update (client, Node::strip_mute, strip_id, strips ().strip_mute (strip_id));
	}
}

void
WebsocketsDispatcher::strip_plugin_enable_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (state.n_addr () < 2) {
		return;
	}

	uint32_t strip_id  = state.nth_addr (0);
	uint32_t plugin_id = state.nth_addr (1);

	if (msg.is_write () && (state.n_val () > 0)) {
		strips ().set_strip_plugin_enabled (strip_id, plugin_id, state.nth_val (0));
	} else {
		update (client, Node::strip_plugin_enable, strip_id, plugin_id,
		        strips ().strip_plugin_enabled (strip_id, plugin_id));
	}
}

void
WebsocketsDispatcher::strip_plugin_param_value_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (state.n_addr () < 3) {
		return;
	}

	uint32_t strip_id  = state.nth_addr (0);
	uint32_t plugin_id = state.nth_addr (1);
	uint32_t param_id  = state.nth_addr (2);

	if (msg.is_write () && (state.n_val () > 0)) {
		strips ().set_strip_plugin_param_value (strip_id, plugin_id, param_id,
		                                        state.nth_val (0));
	} else {
		TypedValue value = strips ().strip_plugin_param_value (strip_id, plugin_id, param_id);
		update (client, Node::strip_plugin_param_value, strip_id, plugin_id, param_id, value);
	}
}

void
WebsocketsDispatcher::update (Client client, std::string node, TypedValue val1)
{
	update (client, node, ADDR_NONE, ADDR_NONE, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_n, TypedValue val1)
{
	update (client, node, strip_n, ADDR_NONE, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_n, uint32_t plugin_n,
                              TypedValue val1)
{
	update (client, node, strip_n, plugin_n, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_n, uint32_t plugin_n,
                              uint32_t param_n, TypedValue val1)
{
	AddressVector addr = AddressVector ();

	if (strip_n != ADDR_NONE) {
		addr.push_back (strip_n);
	}

	if (plugin_n != ADDR_NONE) {
		addr.push_back (plugin_n);
	}

	if (param_n != ADDR_NONE) {
		addr.push_back (param_n);
	}

	ValueVector val = ValueVector ();

	if (!val1.empty ()) {
		val.push_back (val1);
	}

	update (client, node, addr, val);
}

void
WebsocketsDispatcher::update (Client client, std::string node, const AddressVector& addr,
                              const ValueVector& val)
{
	server ().update_client (client, NodeState (node, addr, val), true);
}
