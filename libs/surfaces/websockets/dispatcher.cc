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

#include <boost/assign.hpp>

#include "ardour/plugin_insert.h"

#include "ardour_websockets.h"
#include "dispatcher.h"
#include "state.h"

using namespace ARDOUR;
using namespace ArdourSurface;

#define NODE_METHOD_PAIR(x) (Node::x, &WebsocketsDispatcher::x##_handler)

WebsocketsDispatcher::NodeMethodMap
	WebsocketsDispatcher::_node_to_method = boost::assign::map_list_of
		NODE_METHOD_PAIR (transport_tempo)
		NODE_METHOD_PAIR (transport_roll)
		NODE_METHOD_PAIR (transport_record)
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
	for (ArdourMixer::StripMap::iterator it = mixer().strips().begin(); it != mixer().strips().end(); ++it) {
		uint32_t strip_id        = it->first;
		ArdourMixerStrip& strip = *it->second;

		AddressVector strip_addr = AddressVector ();
		strip_addr.push_back (strip_id);
		
		ValueVector strip_desc = ValueVector ();
		strip_desc.push_back (strip.name ());
		strip_desc.push_back ((int)strip.stripable ()->presentation_info ().flags ());
		
		update (client, Node::strip_description, strip_addr, strip_desc);
		
		update (client, Node::strip_gain, strip_id, strip.gain ());
		update (client, Node::strip_mute, strip_id, strip.mute ());

		if (strip.has_pan ()) {
			update (client, Node::strip_pan, strip_id, strip.pan ());
		}

		for (ArdourMixerStrip::PluginMap::iterator it = strip.plugins ().begin (); it != strip.plugins ().end (); ++it) {
			uint32_t plugin_id                     = it->first;
			boost::shared_ptr<PluginInsert> insert = it->second->insert ();
			boost::shared_ptr<Plugin> plugin       = insert->plugin ();

			update (client, Node::strip_plugin_description, strip_id, plugin_id,
			        static_cast<std::string> (plugin->name ()));

			update (client, Node::strip_plugin_enable, strip_id, plugin_id,
			        strip.plugin (plugin_id).enabled ());

			for (uint32_t param_id = 0; param_id < plugin->parameter_count (); ++param_id) {
				boost::shared_ptr<AutomationControl> a_ctrl;

				try {
				    a_ctrl = strip.plugin (plugin_id).param_control (param_id);
				} catch (ArdourMixerNotFoundException& err) {
					continue;
				}

				AddressVector addr = AddressVector ();
				addr.push_back (strip_id);
				addr.push_back (plugin_id);
				addr.push_back (param_id);

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

				TypedValue value = strip.plugin (plugin_id).param_value (param_id);
				update (client, Node::strip_plugin_param_value, strip_id, plugin_id, param_id, value);
			}
		}
	}

	update (client, Node::transport_tempo, transport ().tempo ());
	update (client, Node::transport_time, transport ().time ());
	update (client, Node::transport_roll, transport ().roll ());
	update (client, Node::transport_record, transport ().record ());
}

void
WebsocketsDispatcher::transport_tempo_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		transport ().set_tempo (state.nth_val (0));
	} else {
		update (client, Node::transport_tempo, transport ().tempo ());
	}
}

void
WebsocketsDispatcher::transport_roll_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		transport ().set_roll (state.nth_val (0));
	} else {
		update (client, Node::transport_roll, transport ().roll ());
	}
}

void
WebsocketsDispatcher::transport_record_handler (Client client, const NodeStateMessage& msg)
{
	const NodeState& state = msg.state ();

	if (msg.is_write () && (state.n_val () > 0)) {
		transport ().set_record (state.nth_val (0));
	} else {
		update (client, Node::transport_record, transport ().record ());
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
		mixer ().strip (strip_id).set_gain (state.nth_val (0));
	} else {
		update (client, Node::strip_gain, strip_id, mixer ().strip (strip_id).gain ());
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
		mixer ().strip (strip_id).set_pan (state.nth_val (0));
	} else {
		update (client, Node::strip_pan, strip_id, mixer ().strip (strip_id).pan ());
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
		mixer ().strip (strip_id).set_mute (state.nth_val (0));
	} else {
		update (client, Node::strip_mute, strip_id, mixer ().strip (strip_id).mute ());
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
		mixer ().strip (strip_id).plugin (plugin_id).set_enabled (state.nth_val (0));
	} else {
		update (client, Node::strip_plugin_enable, strip_id, plugin_id,
		        mixer ().strip (strip_id).plugin (plugin_id).enabled ());
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
		mixer ().strip (strip_id).plugin (plugin_id).set_param_value (param_id,
		                                        state.nth_val (0));
	} else {
		TypedValue value = mixer ().strip (strip_id).plugin (plugin_id).param_value (param_id);
		update (client, Node::strip_plugin_param_value, strip_id, plugin_id, param_id, value);
	}
}

void
WebsocketsDispatcher::update (Client client, std::string node, TypedValue val1)
{
	update (client, node, ADDR_NONE, ADDR_NONE, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_id, TypedValue val1)
{
	update (client, node, strip_id, ADDR_NONE, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_id, uint32_t plugin_id,
                              TypedValue val1)
{
	update (client, node, strip_id, plugin_id, ADDR_NONE, val1);
}

void
WebsocketsDispatcher::update (Client client, std::string node, uint32_t strip_id, uint32_t plugin_id,
                              uint32_t param_id, TypedValue val1)
{
	AddressVector addr = AddressVector ();

	if (strip_id != ADDR_NONE) {
		addr.push_back (strip_id);
	}

	if (plugin_id != ADDR_NONE) {
		addr.push_back (plugin_id);
	}

	if (param_id != ADDR_NONE) {
		addr.push_back (param_id);
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
