/*
    Copyright (C) 2002 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#include <vector>

#include "pbd/debug.h"

#include "ardour/audioengine.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/types_convert.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> fr2997;
		PBD::PropertyDescriptor<bool> sclock_synced;
		PBD::PropertyDescriptor<bool> collect;
		PBD::PropertyDescriptor<bool> connected;
		PBD::PropertyDescriptor<TransportRequestType> allowed_transport_requests;
	}
}

using namespace ARDOUR;
using namespace PBD;

void
TransportMaster::make_property_quarks ()
{
	Properties::fr2997.property_id = g_quark_from_static_string (X_("fr2997"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fr2997 = %1\n", Properties::fr2997.property_id));
	Properties::sclock_synced.property_id = g_quark_from_static_string (X_("sclock_synced"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sclock_synced = %1\n", Properties::sclock_synced.property_id));
	Properties::collect.property_id = g_quark_from_static_string (X_("collect"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for collect = %1\n", Properties::collect.property_id));
	Properties::connected.property_id = g_quark_from_static_string (X_("connected"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for connected = %1\n", Properties::connected.property_id));
	Properties::allowed_transport_requests.property_id = g_quark_from_static_string (X_("allowed_transport_requests"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for allowed_transport_requests = %1\n", Properties::allowed_transport_requests.property_id));
}

const std::string TransportMaster::state_node_name = X_("TransportMaster");

TransportMaster::TransportMaster (SyncSource t, std::string const & name)
	: _type (t)
	, _name (Properties::name, name)
	, _session (0)
	, _current_delta (0)
	, _pending_collect (true)
	, _request_mask (Properties::allowed_transport_requests, TransportRequestType (0))
	, _locked (Properties::locked, false)
	, _sclock_synced (Properties::sclock_synced, false)
	, _collect (Properties::collect, true)
	, _connected (Properties::connected, false)
{
	register_properties ();

	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect_same_thread (port_connection, boost::bind (&TransportMaster::connection_handler, this, _1, _2, _3, _4, _5));
	ARDOUR::AudioEngine::instance()->Running.connect_same_thread (backend_connection, boost::bind (&TransportMaster::check_backend, this));
}

TransportMaster::~TransportMaster()
{
	delete _session;
}

void
TransportMaster::register_properties ()
{
	_xml_node_name = state_node_name;

	add_property (_name);
	add_property (_locked);
	add_property (_collect);
	add_property (_sclock_synced);
	add_property (_request_mask);

	/* we omit _connected since it is derived from port state, and merely
	 * used for signalling
	 */
}

void
TransportMaster::set_name (std::string const & str)
{
	if (_name != str) {
		_name = str;
		PropertyChange (Properties::name);
	}
}

bool
TransportMaster::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	if (!_port) {
		return false;
	}

	const std::string fqn = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (_port->name());

	if (fqn == name1 || fqn == name2) {

		/* it's about us */

		/* XXX technically .. if the user makes an N->1 connection to
		 * this transport master's port, this simple minded logic is
		 * not sufficient. But the user shouldn't do that ...
		 */

		if (yn) {
			_connected = true;
		} else {
			_connected = false;
		}

		PropertyChanged (Properties::connected);

		return true;
	}

	return false;
}

bool
TransportMaster::check_collect()
{
	if (!_connected) {
		return false;
	}

	/* XXX should probably use boost::atomic something or other here */

	if (_pending_collect != _collect) {
		if (_pending_collect) {
			init ();
		} else {
			if (TransportMasterManager::instance().current().get() == this) {
				if (_session) {
					_session->config.set_external_sync (false);
				}
			}
		}
		_collect = _pending_collect;
		PropertyChanged (Properties::collect);
	}

	return _collect;
}

void
TransportMaster::set_collect (bool yn)
{
	_pending_collect = yn;
}

void
TransportMaster::set_sample_clock_synced (bool yn)
{
	if (yn != _sclock_synced) {
		_sclock_synced = yn;
		PropertyChanged (Properties::sclock_synced);
	}
}

void
TransportMaster::set_session (Session* s)
{
	_session = s;
}

int
TransportMaster::set_state (XMLNode const & node, int /* version */)
{
	PropertyChange what_changed;

	what_changed = set_values (node);

	XMLNode* pnode = node.child (X_("Port"));

	if (pnode) {
		XMLNodeList const & children = pnode->children();
		for (XMLNodeList::const_iterator ci = children.begin(); ci != children.end(); ++ci) {

			XMLProperty const *prop;

			if ((*ci)->name() == X_("Connection")) {
				if ((prop = (*ci)->property (X_("other"))) == 0) {
					continue;
				}
				_port->connect (prop->value());
			}
		}
	}

	PropertyChanged (what_changed);

	return 0;
}

XMLNode&
TransportMaster::get_state ()
{
	XMLNode* node = new XMLNode (state_node_name);
	node->set_property (X_("type"), _type);

	add_properties (*node);

	if (_port) {
		std::vector<std::string> connections;

		XMLNode* pnode = new XMLNode (X_("Port"));

		if (_port->get_connections (connections)) {

			std::vector<std::string>::const_iterator ci;
			std::sort (connections.begin(), connections.end());

			for (ci = connections.begin(); ci != connections.end(); ++ci) {

				/* if its a connection to our own port,
				   return only the port name, not the
				   whole thing. this allows connections
				   to be re-established even when our
				   client name is different.
				*/

				XMLNode* cnode = new XMLNode (X_("Connection"));

				cnode->set_property (X_("other"), AudioEngine::instance()->make_port_name_relative (*ci));
				pnode->add_child_nocopy (*cnode);
			}
		}

		node->add_child_nocopy (*pnode);
	}

	return *node;
}

boost::shared_ptr<TransportMaster>
TransportMaster::factory (XMLNode const & node)
{
	if (node.name() != TransportMaster::state_node_name) {
		return boost::shared_ptr<TransportMaster>();
	}

	SyncSource type;
	std::string name;

	if (!node.get_property (X_("type"), type)) {
		return boost::shared_ptr<TransportMaster>();
	}

	if (!node.get_property (X_("name"), name)) {
		return boost::shared_ptr<TransportMaster>();
	}

	return factory (type, name);
}

boost::shared_ptr<TransportMaster>
TransportMaster::factory (SyncSource type, std::string const& name)
{
	/* XXX need to count existing sources of a given type */

	switch (type) {
	case MTC:
		return boost::shared_ptr<TransportMaster> (new MTC_TransportMaster (sync_source_to_string (type)));
	case LTC:
		return boost::shared_ptr<TransportMaster> (new LTC_TransportMaster (sync_source_to_string (type)));
	case MIDIClock:
		return boost::shared_ptr<TransportMaster> (new MIDIClock_TransportMaster (sync_source_to_string (type)));
	case Engine:
		return boost::shared_ptr<TransportMaster> (new Engine_TransportMaster (*AudioEngine::instance()));
	default:
		break;
	}

	return boost::shared_ptr<TransportMaster>();
}

boost::shared_ptr<Port>
TransportMasterViaMIDI::create_midi_port (std::string const & port_name)
{
	boost::shared_ptr<Port> p;

	if ((p = AudioEngine::instance()->register_input_port (DataType::MIDI, port_name)) == 0) {
		return boost::shared_ptr<Port> ();
	}

	_midi_port = boost::dynamic_pointer_cast<MidiPort> (p);

	return p;
}

bool
TransportMaster::allow_request (TransportRequestSource src, TransportRequestType type) const
{
	return _request_mask & type;
}

void
TransportMaster::set_request_mask (TransportRequestType t)
{
	if (_request_mask != t) {
		_request_mask = t;
		PropertyChanged (Properties::allowed_transport_requests);
	}
}

TimecodeTransportMaster::TimecodeTransportMaster (std::string const & name, SyncSource type)
	: TransportMaster (type, name)
	, _fr2997 (Properties::fr2997, false)
{
	register_properties ();
}

void
TimecodeTransportMaster::register_properties ()
{
	TransportMaster::register_properties ();
	add_property (_fr2997);
}

void
TimecodeTransportMaster::set_fr2997 (bool yn)
{
	if (yn != _fr2997) {
		_fr2997 = yn;
		PropertyChanged (Properties::fr2997);
	}
}
