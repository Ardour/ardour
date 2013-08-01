/*
    Copyright (C) 2009 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <jack/weakjack.h> // so that we can test for new functions at runtime

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/port.h"
#include "ardour/port_engine.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal2<void,boost::shared_ptr<Port>, boost::shared_ptr<Port> > Port::PostDisconnect;
PBD::Signal0<void> Port::PortDrop;

bool         Port::_connecting_blocked = false;
pframes_t    Port::_global_port_buffer_offset = 0;
pframes_t    Port::_cycle_nframes = 0;

/* a handy define to shorten what would otherwise be a needlessly verbose
 * repeated phrase
 */
#define port_engine AudioEngine::instance()->port_engine()
#define port_manager AudioEngine::instance()

/** @param n Port short name */
Port::Port (std::string const & n, DataType t, PortFlags f)
	: _port_buffer_offset (0)
	, _name (n)
	, _flags (f)
        , _last_monitor (false)
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

	if ((_port_handle = port_engine.register_port (_name, t, _flags)) == 0) {
		cerr << "Failed to register port \"" << _name << "\", reason is unknown from here\n";
		throw failed_constructor ();
	}
	
	PortDrop.connect_same_thread (drop_connection, boost::bind (&Port::drop, this));
}

/** Port destructor */
Port::~Port ()
{
	drop ();
}

void
Port::drop ()
{
	if (_port_handle) {
		port_engine.unregister_port (_port_handle);
		_port_handle = 0;
	}
}

/** @return true if this port is connected to anything */
bool
Port::connected () const
{
	return (port_engine.connected (_port_handle) != 0);
}

int
Port::disconnect_all ()
{
	port_engine.disconnect_all (_port_handle);
	_connections.clear ();

	/* a cheaper, less hacky way to do boost::shared_from_this() ... 
	 */
	boost::shared_ptr<Port> pself = port_manager->get_port_by_name (name());
	PostDisconnect (pself, boost::shared_ptr<Port>()); // emit signal

	return 0;
}

/** @param o Port name
 * @return true if this port is connected to o, otherwise false.
 */
bool
Port::connected_to (std::string const & o) const
{
	return port_engine.connected_to (_port_handle, AudioEngine::instance()->make_port_name_non_relative (o));
}

int
Port::get_connections (std::vector<std::string> & c) const
{
	return port_engine.get_connections (_port_handle, c);
}

int
Port::connect (std::string const & other)
{
	std::string const other_name = AudioEngine::instance()->make_port_name_non_relative (other);
	std::string const our_name = AudioEngine::instance()->make_port_name_non_relative (_name);

	int r = 0;

	if (_connecting_blocked) {
		return r;
	}

	if (sends_output ()) {
		port_engine.connect (our_name, other_name);
	} else {
		port_engine.connect (other_name, our_name);
	}

	if (r == 0) {
		_connections.insert (other);
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
		_connections.erase (other);
	}

	/* a cheaper, less hacky way to do boost::shared_from_this() ... 
	 */
	boost::shared_ptr<Port> pself = AudioEngine::instance()->get_port_by_name (name());
	boost::shared_ptr<Port> pother = AudioEngine::instance()->get_port_by_name (other);

	if (pself && pother) {
		/* Disconnecting from another Ardour port: need to allow
		   a check on whether this may affect anything that we
		   need to know about.
		*/
		PostDisconnect (pself, pother); // emit signal 
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
	port_engine.request_input_monitoring (_port_handle, yn);
}

void
Port::ensure_input_monitoring (bool yn)
{
	port_engine.ensure_input_monitoring (_port_handle, yn);
}

bool
Port::monitoring_input () const
{
	
	return port_engine.monitoring_input (_port_handle);
}

void
Port::reset ()
{
	_last_monitor = false;
}

void
Port::cycle_start (pframes_t)
{
        _port_buffer_offset = 0;
}

void
Port::increment_port_buffer_offset (pframes_t nframes)
{
        _port_buffer_offset += nframes;
}

void
Port::set_public_latency_range (LatencyRange& range, bool playback) const
{
	/* this sets the visible latency that the rest of the port system
	   sees. because we do latency compensation, all (most) of our visible
	   port latency values are identical.
	*/

	DEBUG_TRACE (DEBUG::Latency,
	             string_compose ("SET PORT %1 %4 PUBLIC latency now [%2 - %3]\n",
	                             name(), range.min, range.max,
	                             (playback ? "PLAYBACK" : "CAPTURE")));;

	port_engine.set_latency_range (_port_handle, playback, range);
}

void
Port::set_private_latency_range (LatencyRange& range, bool playback)
{
	if (playback) {
		_private_playback_latency = range;
		DEBUG_TRACE (DEBUG::Latency, string_compose (
			             "SET PORT %1 playback PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_playback_latency.min,
			             _private_playback_latency.max));
	} else {
		_private_capture_latency = range;
		DEBUG_TRACE (DEBUG::Latency, string_compose (
			             "SET PORT %1 capture PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_capture_latency.min,
			             _private_capture_latency.max));
	}

	/* push to public (port system) location so that everyone else can see it */

	set_public_latency_range (range, playback);
}

const LatencyRange&
Port::private_latency_range (bool playback) const
{
	if (playback) {
		DEBUG_TRACE (DEBUG::Latency, string_compose (
			             "GET PORT %1 playback PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_playback_latency.min,
			             _private_playback_latency.max));
		return _private_playback_latency;
	} else {
		DEBUG_TRACE (DEBUG::Latency, string_compose (
			             "GET PORT %1 capture PRIVATE latency now [%2 - %3]\n",
			             name(),
			             _private_playback_latency.min,
			             _private_playback_latency.max));
		return _private_capture_latency;
	}
}

LatencyRange
Port::public_latency_range (bool /*playback*/) const
{
	LatencyRange r;

	r = port_engine.get_latency_range (_port_handle, sends_output() ? true : false);

	DEBUG_TRACE (DEBUG::Latency, string_compose (
		             "GET PORT %1: %4 PUBLIC latency range %2 .. %3\n",
		             name(), r.min, r.max,
		             sends_output() ? "PLAYBACK" : "CAPTURE"));
	return r;
}

void
Port::get_connected_latency_range (LatencyRange& range, bool playback) const
{
	vector<string> connections;

	get_connections (connections);

	if (!connections.empty()) {

		range.min = ~((jack_nframes_t) 0);
		range.max = 0;

		DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: %2 connections to check for latency range\n", name(), connections.size()));

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

                                        DEBUG_TRACE (DEBUG::Latency, string_compose (
                                                             "\t%1 <-> %2 : latter has latency range %3 .. %4\n",
                                                             name(), *c, lr.min, lr.max));

                                        range.min = min (range.min, lr.min);
                                        range.max = max (range.max, lr.max);
                                }

			} else {

                                /* port belongs to this instance of ardour,
                                   so look up its latency information
                                   internally, because our published/public
                                   values already contain our plugin
                                   latency compensation.
                                */

                                boost::shared_ptr<Port> remote_port = AudioEngine::instance()->get_port_by_name (*c);
                                if (remote_port) {
                                        lr = remote_port->private_latency_range ((playback ? JackPlaybackLatency : JackCaptureLatency));
                                        DEBUG_TRACE (DEBUG::Latency, string_compose (
                                                             "\t%1 <-LOCAL-> %2 : latter has latency range %3 .. %4\n",
                                                             name(), *c, lr.min, lr.max));

                                        range.min = min (range.min, lr.min);
                                        range.max = max (range.max, lr.max);
                                }
                        }
		}

	} else {
		DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: not connected to anything\n", name()));
		range.min = 0;
		range.max = 0;
	}

        DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: final connected latency range [ %2 .. %3 ] \n", name(), range.min, range.max));
}

int
Port::reestablish ()
{
	_port_handle = port_engine.register_port (_name, type(), _flags);

	if (_port_handle == 0) {
		PBD::error << string_compose (_("could not reregister %1"), _name) << endmsg;
		return -1;
	}

	reset ();

	return 0;
}


int
Port::reconnect ()
{
	/* caller must hold process lock; intended to be used only after reestablish() */

	for (std::set<string>::iterator i = _connections.begin(); i != _connections.end(); ++i) {
		if (connect (*i)) {
			return -1;
		}
	}

	return 0;
}

/** @param n Short port name (no port-system client name) */
int
Port::set_name (std::string const & n)
{
	if (n == _name) {
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
	return port_engine.physically_connected (_port_handle);
}

