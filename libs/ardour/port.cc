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

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal2<void,boost::shared_ptr<Port>, boost::shared_ptr<Port> > Port::PostDisconnect;
PBD::Signal0<void> Port::PortDrop;

AudioEngine* Port::_engine = 0;
bool         Port::_connecting_blocked = false;
pframes_t    Port::_global_port_buffer_offset = 0;
pframes_t    Port::_cycle_nframes = 0;

/** @param n Port short name */
Port::Port (std::string const & n, DataType t, Flags f)
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
	   we can create the right kind of JACK port; aside from this we'll use the
	   virtual function type () to establish type.
	*/

	assert (_name.find_first_of (':') == std::string::npos);

	if (!_engine->connected()) {
		throw failed_constructor ();
	}

	if ((_jack_port = jack_port_register (_engine->jack (), _name.c_str (), t.to_jack_type (), _flags, 0)) == 0) {
		cerr << "Failed to register JACK port \"" << _name << "\", reason is unknown from here\n";
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
	if (_jack_port) {
		if (_engine->jack ()) {
			jack_port_unregister (_engine->jack (), _jack_port);
		}
		_jack_port = 0;
	}
}

/** @return true if this port is connected to anything */
bool
Port::connected () const
{
	return (jack_port_connected (_jack_port) != 0);
}

int
Port::disconnect_all ()
{
	jack_port_disconnect (_engine->jack(), _jack_port);
	_connections.clear ();

	/* a cheaper, less hacky way to do boost::shared_from_this() ... 
	 */
	boost::shared_ptr<Port> pself = _engine->get_port_by_name (name());
	PostDisconnect (pself, boost::shared_ptr<Port>()); // emit signal

	return 0;
}

/** @param o Port name
 * @return true if this port is connected to o, otherwise false.
 */
bool
Port::connected_to (std::string const & o) const
{
	if (!_engine->connected()) {
		/* in some senses, this answer isn't the right one all the time,
		   because we know about our connections and will re-establish
		   them when we reconnect to JACK.
		*/
		return false;
	}

	return jack_port_connected_to (_jack_port,
	                               _engine->make_port_name_non_relative(o).c_str ());
}

/** @param o Filled in with port full names of ports that we are connected to */
int
Port::get_connections (std::vector<std::string> & c) const
{
	int n = 0;

	if (_engine->connected()) {
		const char** jc = jack_port_get_connections (_jack_port);
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				c.push_back (jc[i]);
				++n;
			}

                        if (jack_free) {
                                jack_free (jc);
                        } else {
                                free (jc);
                        }
		}
	}

	return n;
}

int
Port::connect (std::string const & other)
{
	std::string const other_shrt = _engine->make_port_name_non_relative (other);
	std::string const this_shrt = _engine->make_port_name_non_relative (_name);

	int r = 0;

	if (_connecting_blocked) {
		return r;
	}

        cerr << "Port::connect " << name() << " <=> " << other << endl;

	if (sends_output ()) {
		r = jack_connect (_engine->jack (), this_shrt.c_str (), other_shrt.c_str ());
	} else {
		r = jack_connect (_engine->jack (), other_shrt.c_str (), this_shrt.c_str());
	}

	if (r == 0) {
		_connections.insert (other);
	}

	return r;
}

int
Port::disconnect (std::string const & other)
{
	std::string const other_fullname = _engine->make_port_name_non_relative (other);
	std::string const this_fullname = _engine->make_port_name_non_relative (_name);

	int r = 0;

	if (sends_output ()) {
		r = jack_disconnect (_engine->jack (), this_fullname.c_str (), other_fullname.c_str ());
	} else {
		r = jack_disconnect (_engine->jack (), other_fullname.c_str (), this_fullname.c_str ());
	}

	if (r == 0) {
		_connections.erase (other);
	}

	/* a cheaper, less hacky way to do boost::shared_from_this() ... 
	 */
	boost::shared_ptr<Port> pself = _engine->get_port_by_name (name());
	boost::shared_ptr<Port> pother = _engine->get_port_by_name (other);

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
Port::set_engine (AudioEngine* e)
{
	_engine = e;
}

void
Port::ensure_jack_monitors_input (bool yn)
{
	jack_port_ensure_monitor (_jack_port, yn);
}

bool
Port::jack_monitoring_input () const
{
	return jack_port_monitoring_input (_jack_port);
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
Port::set_public_latency_range (jack_latency_range_t& range, bool playback) const
{
	/* this sets the visible latency that the rest of JACK sees. because we do latency
	   compensation, all (most) of our visible port latency values are identical.
	*/

	if (!jack_port_set_latency_range) {
		return;
	}

	DEBUG_TRACE (DEBUG::Latency,
	             string_compose ("SET PORT %1 %4 PUBLIC latency now [%2 - %3]\n",
	                             name(), range.min, range.max,
	                             (playback ? "PLAYBACK" : "CAPTURE")));;

	jack_port_set_latency_range (_jack_port,
	                             (playback ? JackPlaybackLatency : JackCaptureLatency),
	                             &range);
}

void
Port::set_private_latency_range (jack_latency_range_t& range, bool playback)
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

	/* push to public (JACK) location so that everyone else can see it */

	set_public_latency_range (range, playback);
}

const jack_latency_range_t&
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

jack_latency_range_t
Port::public_latency_range (bool /*playback*/) const
{
	jack_latency_range_t r;

	jack_port_get_latency_range (_jack_port,
	                             sends_output() ? JackPlaybackLatency : JackCaptureLatency,
	                             &r);
	DEBUG_TRACE (DEBUG::Latency, string_compose (
		             "GET PORT %1: %4 PUBLIC latency range %2 .. %3\n",
		             name(), r.min, r.max,
		             sends_output() ? "PLAYBACK" : "CAPTURE"));
	return r;
}

void
Port::get_connected_latency_range (jack_latency_range_t& range, bool playback) const
{
	if (!jack_port_get_latency_range) {
		return;
	}

	vector<string> connections;
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		range.min = 0;
		range.max = 0;
		PBD::warning << _("get_connected_latency_range() called while disconnected from JACK") << endmsg;
		return;
	}

	get_connections (connections);

	if (!connections.empty()) {

		range.min = ~((jack_nframes_t) 0);
		range.max = 0;

		DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: %2 connections to check for latency range\n", name(), connections.size()));

		for (vector<string>::const_iterator c = connections.begin();
		     c != connections.end(); ++c) {

                        jack_latency_range_t lr;

                        if (!AudioEngine::instance()->port_is_mine (*c)) {

                                /* port belongs to some other JACK client, use
                                 * JACK to lookup its latency information.
                                 */

                                jack_port_t* remote_port = jack_port_by_name (_engine->jack(), (*c).c_str());

                                if (remote_port) {
                                        jack_port_get_latency_range (
                                                remote_port,
                                                (playback ? JackPlaybackLatency : JackCaptureLatency),
                                                &lr);

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
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		return -1;
	}

	_jack_port = jack_port_register (jack, _name.c_str(), type().to_jack_type(), _flags, 0);

	if (_jack_port == 0) {
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

/** @param n Short port name (no JACK client name) */
int
Port::set_name (std::string const & n)
{
	if (n == _name) {
		return 0;
	}

	int const r = jack_port_set_name (_jack_port, n.c_str());

	if (r == 0) {
		_engine->port_renamed (_name, n);
		_name = n;
	}


	return r;
}

void
Port::request_jack_monitors_input (bool yn)
{
	jack_port_request_monitor (_jack_port, yn);
}

bool
Port::physically_connected () const
{
	const char** jc = jack_port_get_connections (_jack_port);

	if (jc) {
		for (int i = 0; jc[i]; ++i) {

			jack_port_t* port = jack_port_by_name (_engine->jack(), jc[i]);

			if (port && (jack_port_flags (port) & JackPortIsPhysical)) {
                                if (jack_free) {
                                        jack_free (jc);
                                } else {
                                        free (jc);
                                }
				return true;
			}
		}
                if (jack_free) {
                        jack_free (jc);
                } else {
                        free (jc);
                }
	}

	return false;
}

