/*
 * Copyright (C) 2018-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#include <vector>

#include "pbd/boost_debug.h"
#include "pbd/debug.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
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
	, _removeable (false)
	, _request_mask (Properties::allowed_transport_requests, TransportRequestType (0))
	, _sclock_synced (Properties::sclock_synced, false)
	, _collect (Properties::collect, true)
	, _connected (Properties::connected, false)
	, port_node (X_("Port"))
{
	register_properties ();

	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect_same_thread (port_connection, boost::bind (&TransportMaster::connection_handler, this, _1, _2, _3, _4, _5));
	ARDOUR::AudioEngine::instance()->Running.connect_same_thread (backend_connection, boost::bind (&TransportMaster::check_backend, this));
}

TransportMaster::~TransportMaster()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("destroying transport master \"%1\" along with port %2\n", name(), (_port ? _port->name() : std::string ("no port"))));

	unregister_port ();
}

bool
TransportMaster::speed_and_position (double& speed, samplepos_t& pos, samplepos_t& lp, samplepos_t& when, samplepos_t now)
{
	if (!_collect) {
		return false;
	}

	if (!locked()) {
		DEBUG_TRACE (DEBUG::Slave, string_compose ("%1: not locked, no speed and position!\n", name()));
		return false;
	}

	SafeTime last;
	current.safe_read (last);

	if (last.timestamp == 0) {
		return false;
	}

	if (last.timestamp && now > last.timestamp && now - last.timestamp > (2.0 * update_interval())) {
		/* no timecode for two cycles - conclude that it's stopped */

		if (!Config->get_transport_masters_just_roll_when_sync_lost()) {
			speed = 0;
			pos = last.position;
			lp = last.position;
			when = last.timestamp;
			_current_delta = 0;
			DEBUG_TRACE (DEBUG::Slave, string_compose ("%1 not seen since %2 vs %3 (%4) with seekahead = %5 reset pending, pos = %6\n", name(), last.timestamp, now, (now - last.timestamp), update_interval(), pos));
			return false;
		}
	}

	lp = last.position;
	when = last.timestamp;
	speed = last.speed;

	/* provide a .1% deadzone to lock the speed */
	if (fabs (speed - 1.0) <= 0.001) {
		speed = 1.0;
	}

	pos = last.position + (now - last.timestamp) * speed;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("%1 sync spd: %2 pos: %3 | last-pos: %4 @  %7 | elapsed: %5 | speed: %6\n",
	                                           name(), speed, pos, last.position, (now - last.timestamp), speed, when));

	return true;
}

void
TransportMaster::register_properties ()
{
	_xml_node_name = state_node_name;

	add_property (_name);
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
		PropertyChanged (Properties::name);
	}
}

void
TransportMaster::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string, boost::weak_ptr<ARDOUR::Port> w1, std::string, bool yn)
{
	if (!_port) {
		return;
	}

	boost::shared_ptr<Port> p = w1.lock ();
	if (p == _port) {
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
	}
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
	/* theoretical race condition */

	if (_connected) {
		_pending_collect = yn;
	} else {
		if (_collect != yn) {
			_pending_collect = _collect = yn;
			PropertyChanged (Properties::collect);
		}
	}
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
	if (!_session) {
		unregister_port ();
	}
}

int
TransportMaster::set_state (XMLNode const & node, int /* version */)
{
	PropertyChange what_changed;

	what_changed = set_values (node);

	XMLNode* pnode = node.child (X_("Port"));

	if (pnode) {
		port_node = *pnode;

		if (AudioEngine::instance()->running()) {
			connect_port_using_state ();
		}
	}

	PropertyChanged (what_changed);

	return 0;
}

void
TransportMaster::connect_port_using_state ()
{
	if (!_port) {
		create_port ();
	}

	if (_port) {
		XMLNodeList const & children = port_node.children();
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
}

XMLNode&
TransportMaster::get_state ()
{
	XMLNode* node = new XMLNode (state_node_name);
	node->set_property (X_("type"), _type);
	node->set_property (X_("removeable"), _removeable);

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

		port_node = *pnode;
		node->add_child_nocopy (*pnode);
	} else if (port_node.children (). size() > 0) {
		node->add_child_copy (port_node);
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
	bool removeable;

	if (!node.get_property (X_("type"), type)) {
		return boost::shared_ptr<TransportMaster>();
	}

	if (!node.get_property (X_("name"), name)) {
		return boost::shared_ptr<TransportMaster>();
	}

	if (!node.get_property (X_("removeable"), removeable)) {
		/* development versions of 6.0 didn't have this property for a
		   while. Any TM listed in XML at that time was non-removeable
		*/
		removeable = false;
	}

	DEBUG_TRACE (DEBUG::Slave, string_compose ("xml-construct %1 name %2 removeable %3\n", enum_2_string (type), name, removeable));

	return factory (type, name, removeable);
}

boost::shared_ptr<TransportMaster>
TransportMaster::factory (SyncSource type, std::string const& name, bool removeable)
{
	/* XXX need to count existing sources of a given type */

	boost::shared_ptr<TransportMaster> tm;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("factory-construct %1 name %2 removeable %3\n", enum_2_string (type), name, removeable));

	try {
		switch (type) {
		case MTC:
			tm.reset (new MTC_TransportMaster (name));
			break;
		case LTC:
			tm.reset (new LTC_TransportMaster (name));
			break;
		case MIDIClock:
			tm.reset (new MIDIClock_TransportMaster (name));
			break;
		case Engine:
			tm.reset (new Engine_TransportMaster (*AudioEngine::instance()));
			break;
		default:
			break;
		}
	} catch (...) {
		error << string_compose (_("Construction of transport master object of type %1 failed"), enum_2_string (type)) << endmsg;
		std::cerr << string_compose (_("Construction of transport master object of type %1 failed"), enum_2_string (type)) << std::endl;
		return boost::shared_ptr<TransportMaster>();
	}

	if (tm) {
		if (AudioEngine::instance()->running()) {
			tm->create_port ();
		}
		tm->set_removeable (removeable);
	}

	return tm;
}

/** @param sh Return a short version of the string */
std::string
TransportMaster::display_name (bool sh) const
{

	switch (_type) {
	case Engine:
		/* no other backends offer sync for now ... deal with this if we
		 * ever have to.
		 */
		return S_("SyncSource|JACK");

	case MTC:
		if (sh) {
			if (name().length() <= 4) {
				return name();
			}
			return S_("SyncSource|MTC");
		} else {
			return name();
		}

	case MIDIClock:
		if (sh) {
			if (name().length() <= 4) {
				return name();
			}
			return S_("SyncSource|M-Clk");
		} else {
			return name();
		}

	case LTC:
		if (sh) {
			if (name().length() <= 4) {
				return name();
			}
			return S_("SyncSource|LTC");
		} else {
			return name();
		}
	}
	/* GRRRR .... stupid, stupid gcc - you can't get here from there, all enum values are handled */
	return S_("SyncSource|JACK");
}

void
TransportMaster::unregister_port ()
{
	if (_port) {
		AudioEngine::instance()->unregister_port (_port);
		_port.reset ();
	}
}

bool
TransportMaster::allow_request (TransportRequestSource src, TransportRequestType type) const
{
	return _request_mask & type;
}

std::string
TransportMaster::allowed_request_string () const
{
	std::string s;
	if (_request_mask == TransportRequestType (TR_StartStop|TR_Speed|TR_Locate)) {
		s = _("All");
	} else if (_request_mask == TransportRequestType (0)) {
		s = _("None");
	} else if (_request_mask == TR_StartStop) {
		s = _("Start/Stop");
	} else if (_request_mask == TR_Speed) {
		s = _("Speed");
	} else if (_request_mask == TR_Locate) {
		s = _("Locate");
	} else {
		s = _("Complex");
	}

	return s;
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
	, timecode_offset (0)
	, timecode_negative_offset (false)
	, timecode_format_valid (false)
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

/* used for delta_string(): (note: \u00B1 is the plus-or-minus sign) */
#define PLUSMINUS(A) (((A) < 0) ? "-" : (((A) > 0) ? "+" : "\u00B1"))
#define LEADINGZERO(A) ((A) < 10 ? "    " : (A) < 100 ? "   " : (A) < 1000 ? "  " : (A) < 10000 ? " " : "")

std::string
TransportMaster::format_delta_time (sampleoffset_t delta) const
{
	char buf[64];
	if (_session) {
		samplecnt_t sr = _session->sample_rate();
		if (abs (_current_delta) >= sr) {
			int secs = rint ((double) delta / sr);
			snprintf(buf, sizeof(buf), "\u0394%s%s%d s", LEADINGZERO(abs(secs)), PLUSMINUS(-secs), abs(secs));
			buf[63] = '\0';
			return std::string(buf);
		}
	}
	/* left-align sign, to make it readable when decimals jitter */
	snprintf (buf, sizeof(buf), "\u0394%s%s%lldsm", PLUSMINUS(-delta), LEADINGZERO(::llabs(delta)), ::llabs(delta));
	buf[63] = '\0';
	return std::string(buf);
}

TransportMasterViaMIDI::~TransportMasterViaMIDI ()
{
	session_connections.drop_connections();
}

boost::shared_ptr<Port>
TransportMasterViaMIDI::create_midi_port (std::string const & port_name)
{
	boost::shared_ptr<Port> p;

	if ((p = AudioEngine::instance()->register_input_port (DataType::MIDI, port_name, false, TransportMasterPort)) == 0) {
		return boost::shared_ptr<Port> ();
	}

	_midi_port = boost::dynamic_pointer_cast<MidiPort> (p);

	return p;
}

void
TransportMasterViaMIDI::set_session (Session* s)
{
	session_connections.drop_connections();

	if (!s) {
		return;
	}

	s->config.ParameterChanged.connect_same_thread (session_connections, boost::bind (&TransportMasterViaMIDI::parameter_changed, this, _1));
	s->LatencyUpdated.connect_same_thread (session_connections, boost::bind (&TransportMasterViaMIDI::resync_latency, this, _1));
}

void
TransportMasterViaMIDI::resync_latency (bool playback)
{
	if (playback) {
		return;
	}

	if (_midi_port) {
		_midi_port->get_connected_latency_range (midi_port_latency, false);
	}
}
