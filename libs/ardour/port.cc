/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/port.h"
#include "ardour/port_engine.h"
#include "ardour/rc_configuration.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal0<void> Port::PortDrop;
PBD::Signal0<void> Port::PortSignalDrop;
PBD::Signal0<void> Port::ResamplerQualityChanged;

bool         Port::_connecting_blocked = false;
pframes_t    Port::_global_port_buffer_offset = 0;
pframes_t    Port::_cycle_nframes = 0;
double       Port::_speed_ratio = 1.0;
double       Port::_engine_ratio = 1.0;
double       Port::_resample_ratio = 1.0;
std::string  Port::state_node_name = X_("Port");
uint32_t     Port::_resampler_quality = 17;
uint32_t     Port::_resampler_latency = 16; // = _resampler_quality - 1;

/* a handy define to shorten what would otherwise be a needlessly verbose
 * repeated phrase
 */
#define port_engine AudioEngine::instance()->port_engine()
#define port_manager AudioEngine::instance()

/** @param n Port short name */
Port::Port (std::string const & n, DataType t, PortFlags f)
	: _name (n)
	, _flags (f)
	, _last_monitor (false)
	, _in_cycle (false)
	, _externally_connected (0)
	, _internally_connected (0)
{
	_private_playback_latency.min = 0;
	_private_playback_latency.max = 0;
	_private_capture_latency.min = 0;
	_private_capture_latency.max = 0;

	/* Unfortunately we have to pass the DataType into this constructor so that
	   we can create the right kind of port; aside from this we'll use the
	   virtual function type () to establish type.
	*/

	assert (_name.find_first_of (':') == std::string::npos);

	if (!port_manager->running ()) {
		DEBUG_TRACE (DEBUG::Ports, string_compose ("port-engine n/a postpone registering %1\n", name()));
		_port_handle.reset (); // created during ::reestablish() later
	} else if ((_port_handle = port_engine.register_port (_name, t, _flags)) == 0) {
		cerr << "Failed to register port \"" << _name << "\", reason is unknown from here\n";
		throw failed_constructor ();
	}
	DEBUG_TRACE (DEBUG::Ports, string_compose ("registered port %1 handle %2\n", name(), _port_handle));

	PortDrop.connect_same_thread (drop_connection, boost::bind (&Port::session_global_drop, this));
	PortSignalDrop.connect_same_thread (drop_connection, boost::bind (&Port::signal_drop, this));
	port_manager->PortConnectedOrDisconnected.connect_same_thread (engine_connection, boost::bind (&Port::port_connected_or_disconnected, this, _1, _2, _3, _4, _5));
}

/** Port destructor */
Port::~Port ()
{
	DEBUG_TRACE (PBD::DebugBits (DEBUG::Destruction|DEBUG::Ports), string_compose ("destroying port @ %1 named %2\n", this, name()));
	drop ();
}

std::string
Port::pretty_name(bool fallback_to_name) const
{
	if (_port_handle) {
		std::string value;
		std::string type;
		if (0 == port_engine.get_port_property (_port_handle,
					"http://jackaudio.org/metadata/pretty-name",
					value, type))
		{
			return value;
		}
	}
	if (fallback_to_name) {
		return name ();
	}
	return "";
}

bool
Port::set_pretty_name(const std::string& n)
{
	if (_port_handle) {
		if (0 == port_engine.set_port_property (_port_handle,
					"http://jackaudio.org/metadata/pretty-name", n, ""))
		{
			return true;
		}
	}
	return false;
}

void
Port::session_global_drop()
{
	if (_flags & TransportMasterPort) {
		return;
	}

	drop ();
}

void
Port::signal_drop ()
{
	engine_connection.disconnect ();
}

void
Port::drop ()
{
	if (_port_handle) {
		DEBUG_TRACE (DEBUG::Ports, string_compose ("drop handle for port %1\n", name()));
		port_engine.unregister_port (_port_handle);
		_port_handle.reset ();
	}
}

void
Port::port_connected_or_disconnected (std::weak_ptr<Port> w0, std::string n1, std::weak_ptr<Port> w1, std::string n2, bool con)
{
	std::shared_ptr<Port> p0 = w0.lock ();
	std::shared_ptr<Port> p1 = w1.lock ();
	/* a cheaper, less hacky way to do boost::shared_from_this() ...  */
	std::shared_ptr<Port> pself = AudioEngine::instance()->get_port_by_name (name());

	if (p0 == pself) {
		if (con) {
			insert_connection (n2);
		} else {
			erase_connection (n2);
		}
		ConnectedOrDisconnected (p0, p1, con); // emit signal
	}
	if (p1 == pself) {
		if (con) {
			insert_connection (n1);
		} else {
			erase_connection (n1);
		}
		ConnectedOrDisconnected (p1, p0, con); // emit signal
	}
}

void
Port::insert_connection (std::string const& pn)
{
#if 1 // include external JACK clients
	if (!AudioEngine::instance()->port_is_mine (pn))
#else
	if (port_manager->port_is_physical (pn))
#endif
	{
		std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));
		Glib::Threads::RWLock::WriterLock lm (_connections_lock);
		_ext_connections[bid].insert (pn);
		_int_connections.erase (pn); // XXX
	} else {
		Glib::Threads::RWLock::WriterLock lm (_connections_lock);
		_int_connections.insert (pn);
	}
}

void
Port::erase_connection (std::string const& pn)
{
#if 1 // include external JACK clients
	if (!AudioEngine::instance()->port_is_mine (pn))
#else
	if (port_manager->port_is_physical (pn))
#endif
	{
		std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));
		Glib::Threads::RWLock::WriterLock lm (_connections_lock);
		if (_ext_connections.find (bid) != _ext_connections.end ()) {
			_ext_connections[bid].erase (pn);
		}
	} else {
		Glib::Threads::RWLock::WriterLock lm (_connections_lock);
		_int_connections.erase (pn);
	}
}

void
Port::increment_external_connections ()
{
	_externally_connected++;
}

void
Port::decrement_external_connections ()
{
	if (_externally_connected) {
		_externally_connected--;
	}
}

void
Port::increment_internal_connections ()
{
	_internally_connected++;
}

void
Port::decrement_internal_connections ()
{
	if (_internally_connected) {
		_internally_connected--;
	}
}

/** @return true if this port is connected to anything */
bool
Port::connected () const
{
	if (_port_handle) {
		return (port_engine.connected (_port_handle) != 0);
	}
	return false;
}

int
Port::disconnect_all ()
{
	if (_port_handle) {

		std::vector<std::string> connections;
		get_connections (connections);

		port_engine.disconnect_all (_port_handle);
		{
			std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));
			Glib::Threads::RWLock::WriterLock lm (_connections_lock);
			_int_connections.clear ();
			if (_ext_connections.find (bid) != _ext_connections.end ()) {
				_ext_connections[bid].clear ();
			}
		}

		/* a cheaper, less hacky way to do boost::shared_from_this() ...
		 */
		std::shared_ptr<Port> pself = port_manager->get_port_by_name (name());
		for (vector<string>::const_iterator c = connections.begin(); c != connections.end() && pself; ++c) {
			std::shared_ptr<Port> pother = AudioEngine::instance()->get_port_by_name (*c);
			if (pother) {
				pother->erase_connection (_name);
				ConnectedOrDisconnected (pself, pother, false); // emit signal
			}
		}
	}

	return 0;
}

/** @param o Port name
 * @return true if this port is connected to o, otherwise false.
 */
bool
Port::connected_to (std::string const & o) const
{
	if (!_port_handle) {
		return false;
	}

	if (!port_manager->running()) {
		return false;
	}

	return port_engine.connected_to (_port_handle, AudioEngine::instance()->make_port_name_non_relative (o), true);
}

int
Port::get_connections (std::vector<std::string>& c) const
{
	if (!port_manager->running()) {
		std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));
		Glib::Threads::RWLock::ReaderLock lm (_connections_lock);
		c.insert (c.end(), _int_connections.begin(), _int_connections.end());
		if (_ext_connections.find (bid) != _ext_connections.end ()) {
			c.insert (c.end(), _ext_connections.at(bid).begin(), _ext_connections.at(bid).end());
		}
		return c.size ();
	}

	if (_port_handle) {
		return port_engine.get_connections (_port_handle, c);
	}

	return 0;
}

int
Port::connect_internal (std::string const & other)
{
	std::string const other_name = AudioEngine::instance()->make_port_name_non_relative (other);
	std::string const our_name = AudioEngine::instance()->make_port_name_non_relative (_name);

	int r = 0;

	if (_connecting_blocked) {
		return r;
	}

	if (sends_output ()) {
		DEBUG_TRACE (DEBUG::Ports, string_compose ("Connect %1 to %2\n", our_name, other_name));
		r = port_engine.connect (our_name, other_name);
	} else {
		DEBUG_TRACE (DEBUG::Ports, string_compose ("Connect %1 to %2\n", other_name, our_name));
		r = port_engine.connect (other_name, our_name);
	}
	return r;
}

int
Port::connect (std::string const& other)
{
	int r = connect_internal (other);

	if (r == 0) {
		/* Connections can be saved on either or both sides. The code above works regardless
		 * from which end the connection is initiated, and connecting already connected ports
		 * is idempotent.
		 *
		 * Only saving internal connection on the source-side would be preferable,
		 * but this is not what JACK does :(
		 * Port::get_state() calls Port::get_connections() which in case of JACK is symmetric.
		 *
		 * This is also nicer when reading the session file's <Port><Connection>.
		 */
		insert_connection (other);

		std::shared_ptr<Port> pother = AudioEngine::instance()->get_port_by_name (other);
		if (pother) {
			pother->insert_connection (_name);
		}
	}

	return r;
}

int
Port::disconnect (std::string const & other)
{
	std::string const other_fullname = port_manager->make_port_name_non_relative (other);
	std::string const this_fullname = port_manager->make_port_name_non_relative (_name);

	int r = 0;

	if (sends_output ()) {
		r = port_engine.disconnect (this_fullname, other_fullname);
	} else {
		r = port_engine.disconnect (other_fullname, this_fullname);
	}

	if (r == 0) {
		erase_connection (other);
	}

	/* a cheaper, less hacky way to do boost::shared_from_this() ...  */
	std::shared_ptr<Port> pself = AudioEngine::instance()->get_port_by_name (name());
	std::shared_ptr<Port> pother = AudioEngine::instance()->get_port_by_name (other);

	if (r == 0 && pother) {
		pother->erase_connection (_name);
	}

	if (pself && pother) {
		/* Disconnecting from another Ardour port: need to allow
		   a check on whether this may affect anything that we
		   need to know about.
		*/
		ConnectedOrDisconnected (pself, pother, false); // emit signal
	}

	return r;
}


bool
Port::connected_to (Port* o) const
{
	return connected_to (o->name ());
}

int
Port::connect (Port* o)
{
	return connect (o->name ());
}

int
Port::disconnect (Port* o)
{
	return disconnect (o->name ());
}

void
Port::request_input_monitoring (bool yn)
{
	if (_port_handle) {
		port_engine.request_input_monitoring (_port_handle, yn);
	}
}

void
Port::ensure_input_monitoring (bool yn)
{
	if (_port_handle) {
		port_engine.ensure_input_monitoring (_port_handle, yn);
	}
}

bool
Port::monitoring_input () const
{
	if (_port_handle) {
		return port_engine.monitoring_input (_port_handle);
	}
	return false;
}

void
Port::reset ()
{
	_last_monitor = false;
	_externally_connected = 0;
}

void
Port::cycle_start (pframes_t)
{
	assert (!_in_cycle);
	_in_cycle = true;
}

void
Port::cycle_end (pframes_t)
{
	assert (_in_cycle);
	_in_cycle = false;
}


void
Port::set_public_latency_range (LatencyRange const& range, bool playback) const
{
	/* this sets the visible latency that the rest of the port system
	   sees. because we do latency compensation, all (most) of our visible
	   port latency values are identical.
	*/

	DEBUG_TRACE (DEBUG::LatencyIO,
	             string_compose ("SET PORT %1 %4 PUBLIC latency now [%2 - %3]\n",
	                             name(), range.min, range.max,
	                             (playback ? "PLAYBACK" : "CAPTURE")));;

	if (_port_handle) {
		LatencyRange r (range);
		if (externally_connected () && 0 == (_flags & TransportSyncPort) && sends_output () == playback) {
			if (type () == DataType::AUDIO) {
				r.min += (_resampler_latency);
				r.max += (_resampler_latency);
			}
		}
		port_engine.set_latency_range (_port_handle, playback, r);
	}
}

void
Port::set_private_latency_range (LatencyRange& range, bool playback)
{
	if (playback) {
		_private_playback_latency = range;
		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
			             "SET PORT %1 playback PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_playback_latency.min,
			             _private_playback_latency.max));
	} else {
		_private_capture_latency = range;
		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
			             "SET PORT %1 capture PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_capture_latency.min,
			             _private_capture_latency.max));
	}
}

const LatencyRange&
Port::private_latency_range (bool playback) const
{
	if (playback) {
		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
			             "GET PORT %1 playback PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_playback_latency.min,
			             _private_playback_latency.max));
		return _private_playback_latency;
	} else {
		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
			             "GET PORT %1 capture PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_capture_latency.min,
			             _private_capture_latency.max));
		return _private_capture_latency;
	}
}

LatencyRange
Port::public_latency_range (bool playback) const
{
	/*Note: this method is no longer used. It exists purely for debugging reasons */
	LatencyRange r;

	if (_port_handle) {
		r = port_engine.get_latency_range (_port_handle, playback);

		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
				     "GET PORT %1: %4 PUBLIC latency range %2 .. %3\n",
				     name(), r.min, r.max,
				     playback ? "PLAYBACK" : "CAPTURE"));
	}

	return r;
}

void
Port::collect_latency_from_backend (LatencyRange& range, bool playback) const
{
	vector<string> connections;
	get_connections (connections);

	DEBUG_TRACE (DEBUG::LatencyIO, string_compose ("%1: %2 connections to check for real %3 latency range\n",
	                                               name(), connections.size(),
	                                               playback ? "PLAYBACK" : "CAPTURE"));

	for (vector<string>::const_iterator c = connections.begin(); c != connections.end(); ++c) {
		PortEngine::PortHandle ph = port_engine.get_port_by_name (*c);
		if (!ph) {
			continue;
		}

		LatencyRange lr = port_engine.get_latency_range (ph, playback);

		if (!AudioEngine::instance()->port_is_mine (*c)) {
			if (externally_connected () && 0 == (_flags & TransportSyncPort) && sends_output () == playback) {
				if (type () == DataType::AUDIO) {
					lr.min += (_resampler_latency);
					lr.max += (_resampler_latency);
				}
			}
		}

		DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
					"\t%1 <-> %2 : latter has latency range %3 .. %4\n",
					name(), *c, lr.min, lr.max));

		range.min = min (range.min, lr.min);
		range.max = max (range.max, lr.max);
	}

	DEBUG_TRACE (DEBUG::LatencyIO, string_compose ("%1: real latency range now [ %2 .. %3 ] \n", name(), range.min, range.max));
}

void
Port::get_connected_latency_range (LatencyRange& range, bool playback) const
{
	vector<string> connections;

	get_connections (connections);

	if (!connections.empty()) {

		range.min = ~((pframes_t) 0);
		range.max = 0;

		DEBUG_TRACE (DEBUG::LatencyIO, string_compose ("%1: %2 connections to check for %3 latency range\n",
		                                               name(), connections.size(),
		                                               playback ? "PLAYBACK" : "CAPTURE"));

		for (vector<string>::const_iterator c = connections.begin();
				c != connections.end(); ++c) {

			LatencyRange lr;

			if (!AudioEngine::instance()->port_is_mine (*c)) {

				/* port belongs to some other port-system client, use
				 * the port engine to lookup its latency information.
				 */

				PortEngine::PortHandle remote_port = port_engine.get_port_by_name (*c);

				if (remote_port) {
					lr = port_engine.get_latency_range (remote_port, playback);
					if (externally_connected () && 0 == (_flags & TransportSyncPort) && sends_output () == playback) {
						if (type () == DataType::AUDIO) {
							lr.min += (_resampler_latency);
							lr.max += (_resampler_latency);
						}
					}

					DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
								"\t%1 <-> %2 : latter has latency range %3 .. %4\n",
								name(), *c, lr.min, lr.max));

					range.min = min (range.min, lr.min);
					range.max = max (range.max, lr.max);
				}

			} else {

				/* port belongs to this instance of ardour,
				 * so look up its latency information
				 * internally, because our published/public
				 * values already contain our plugin
				 * latency compensation.
				 */

				std::shared_ptr<Port> remote_port = AudioEngine::instance()->get_port_by_name (*c);
				if (remote_port) {
					lr = remote_port->private_latency_range (playback);
					DEBUG_TRACE (DEBUG::LatencyIO, string_compose (
								"\t%1 <-LOCAL-> %2 : latter has private latency range %3 .. %4\n",
								name(), *c, lr.min, lr.max));

					range.min = min (range.min, lr.min);
					range.max = max (range.max, lr.max);
				}
			}
		}

	} else {
		DEBUG_TRACE (DEBUG::LatencyIO, string_compose ("%1: not connected to anything\n", name()));
		range.min = 0;
		range.max = 0;
	}

	DEBUG_TRACE (DEBUG::LatencyIO, string_compose ("%1: final connected latency range [ %2 .. %3 ] \n", name(), range.min, range.max));
}

int
Port::reestablish ()
{
	DEBUG_TRACE (DEBUG::Ports, string_compose ("re-establish %1 port %2\n", type().to_string(), _name));
	_port_handle = port_engine.register_port (_name, type(), _flags);

	if (_port_handle == 0) {
		PBD::error << string_compose (_("could not reregister %1"), _name) << endmsg;
		return -1;
	}

	DEBUG_TRACE (DEBUG::Ports, string_compose ("Port::reestablish %1 handle %2\n", name(), _port_handle));

	reset ();

	port_manager->PortConnectedOrDisconnected.connect_same_thread (engine_connection, boost::bind (&Port::port_connected_or_disconnected, this, _1, _2, _3, _4, _5));
	return 0;
}

bool
Port::has_ext_connection () const
{
	std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));

	Glib::Threads::RWLock::ReaderLock lm (_connections_lock);

	return _ext_connections.find (bid) != _ext_connections.end ();
}

int
Port::reconnect ()
{
	/* caller must hold process lock; intended to be used only after reestablish() */

	int count = 0;

	std::string const bid (AudioEngine::instance()->backend_id (receives_input ()));

	Glib::Threads::RWLock::WriterLock lm (_connections_lock);

	if (_ext_connections.find (bid) != _ext_connections.end ()) {
		if (_int_connections.empty () && _ext_connections[bid].empty ()) {
			return 0; /* OK */
		}
		count = _int_connections.size() + _ext_connections[bid].size();
	} else {
		if (_int_connections.empty ()) {
			return 0; /* OK */
		}
		count = _int_connections.size();
	}

	DEBUG_TRACE (DEBUG::Ports, string_compose ("Port::reconnect() Connect %1 to %2 destinations\n",name(), count));

	count = 0;

	ConnectionSet::iterator i = _int_connections.begin();
	while (i != _int_connections.end()) {
		ConnectionSet::iterator current = i++;
		if (connect_internal (*current)) {
			DEBUG_TRACE (DEBUG::Ports, string_compose ("Port::reconnect() failed to connect %1 to %2\n", name(), (*current)));
			_int_connections.erase (current);
		} else {
			++count;
		}
	}

	if (_ext_connections.find (bid) != _ext_connections.end ()) {
		i = _ext_connections[bid].begin();
		while (i != _ext_connections[bid].end()) {
			ConnectionSet::iterator current = i++;
			if (connect_internal (*current)) {
				DEBUG_TRACE (DEBUG::Ports, string_compose ("Port::reconnect() failed to connect %1 to %2\n", name(), (*current)));
				_ext_connections[bid].erase (current);
			} else {
				++count;
			}
		}
	}

	return count == 0 ? -1 : 0;
}

/** @param n Short port name (no port-system client name) */
int
Port::set_name (std::string const & n)
{
	if (n == _name || !_port_handle) {
		return 0;
	}

	int const r = port_engine.set_port_name (_port_handle, n);

	if (r == 0) {
		AudioEngine::instance()->port_renamed (_name, n);
		_name = n;
	}


	return r;
}

bool
Port::physically_connected () const
{
	if (!_port_handle) {
		return false;
	}

	return port_engine.physically_connected (_port_handle);
}

XMLNode&
Port::get_state () const
{
	XMLNode* root = new XMLNode (state_node_name);

	root->set_property (X_("name"), AudioEngine::instance()->make_port_name_relative (name()));
	root->set_property (X_("type"), type ());

	if (receives_input()) {
		root->set_property (X_("direction"), X_("Input"));
	} else {
		root->set_property (X_("direction"), X_("Output"));
	}

	Glib::Threads::RWLock::ReaderLock lm (_connections_lock);
	for (auto const& c : _int_connections) {
		XMLNode* child = new XMLNode (X_("Connection"));
		child->set_property (X_("other"), AudioEngine::instance()->make_port_name_relative (c));
		root->add_child_nocopy (*child);
	}

	for (auto const& hwc : _ext_connections) {
		XMLNode* child = new XMLNode (X_("ExtConnection"));
		child->set_property (X_("for"), hwc.first);
		root->add_child_nocopy (*child);
		for (auto const& c : hwc.second) {
			XMLNode* child = new XMLNode (X_("ExtConnection"));
			child->set_property (X_("for"), hwc.first);
			child->set_property (X_("other"), c);
			root->add_child_nocopy (*child);
		}
	}

	return *root;
}

int
Port::set_state (const XMLNode& node, int)
{
	if (node.name() != state_node_name) {
		return -1;
	}

	std::string str;
	if (node.get_property (X_("name"), str)) {
		set_name (str);
	}

	const XMLNodeList& children (node.children());

	_int_connections.clear ();
	_ext_connections.clear ();

	for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {

		if ((*c)->name() == X_("Connection") && (*c)->get_property (X_("other"), str)) {
			_int_connections.insert (AudioEngine::instance()->make_port_name_non_relative (str));
			continue;
		}

		std::string hw;
		if ((*c)->name() == X_("ExtConnection") && (*c)->get_property (X_("for"), hw)) {
			if ((*c)->get_property (X_("other"), str)) {
			_ext_connections[hw].insert (str);
			} else {
			_ext_connections[hw]; // create
			}
		}
	}

	return 0;
}

/* static */ bool
Port::setup_resampler (uint32_t q)
{
	uint32_t cur_quality = _resampler_quality;

	if (q == 0) {
		/* no vari-speed */
		_resampler_quality = 0;
		_resampler_latency = 0;
	} else {
		/* range constrained in VMResampler::setup */
		if (q < 8) {
			q = 8;
		}
		if (q > 96) {
			q = 96;
		}
		_resampler_quality = q;
		_resampler_latency = q - 1;
	}

	if (cur_quality != _resampler_quality) {
		ResamplerQualityChanged (); /* EMIT SIGNAL */
		if (port_manager) {
			Glib::Threads::Mutex::Lock lm (port_manager->process_lock ());
			port_manager->reinit (true);
			return false;
		}
	}
	return true;
}

/*static*/ bool
Port::set_engine_ratio (double session_rate, double engine_rate)
{
	bool rv = true;
	if (session_rate > 0 && engine_rate > 0 && can_varispeed ()) {
		_engine_ratio = session_rate / engine_rate;
	} else {
		_engine_ratio = 1.0;
		rv = false;
	}

	/* constrain range to provide for additional vari-speed.
	 * but do allow 384000 / 44100 = 8.7
	 */
	if (_engine_ratio < 0.11 || _engine_ratio > 9) {
		_engine_ratio = 1.0;
		rv = false;
	}

	/* apply constraints, and calc _resample_ratio */
	set_varispeed_ratio (_speed_ratio);
	return rv;
}

/*static*/ void
Port::set_varispeed_ratio (double s) {
	if (s == 0.0 || !can_varispeed ()) {
		/* no resampling when stopped */
		_speed_ratio = 1.0;
	} else {
		/* see VMResampler::set_rratio() for min/max range */
		_speed_ratio = std::min (16.0, std::max (0.02, fabs (s * _engine_ratio))) / _engine_ratio;
		_speed_ratio = std::min ((double) Config->get_max_transport_speed(), _speed_ratio);
	}
	/* cache overall speed */
	_resample_ratio = _speed_ratio * _engine_ratio;
}

/*static*/ void
Port::set_cycle_samplecnt (pframes_t n)
{
	_cycle_nframes = floor (n * resample_ratio ());
}
