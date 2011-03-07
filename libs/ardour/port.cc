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

#include <stdexcept>

#include <jack/weakjack.h> // so that we can test for new functions at runtime

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/debug.h"
#include "ardour/port.h"
#include "ardour/audioengine.h"
#include "pbd/failed_constructor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioEngine* Port::_engine = 0;
pframes_t Port::_buffer_size = 0;
bool Port::_connecting_blocked = false;
framecnt_t Port::_port_offset = 0;

/** @param n Port short name */
Port::Port (std::string const & n, DataType t, Flags f)
	: _last_monitor (false)
	, _name (n)
	, _flags (f)
{

	/* Unfortunately we have to pass the DataType into this constructor so that we can
	   create the right kind of JACK port; aside from this we'll use the virtual function type ()
	   to establish type.
	*/

	assert (_name.find_first_of (':') == std::string::npos);

	if (!_engine->connected()) {
		throw failed_constructor ();
	}

	if ((_jack_port = jack_port_register (_engine->jack (), _name.c_str (), t.to_jack_type (), _flags, 0)) == 0) {
                cerr << "Failed to register JACK port, reason is unknown from here\n";
		throw failed_constructor ();
	}
}

/** Port destructor */
Port::~Port ()
{
	if (_engine->jack ()) {
		jack_port_unregister (_engine->jack (), _jack_port);
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

	return jack_port_connected_to (_jack_port, _engine->make_port_name_non_relative(o).c_str ());
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
                        
                        jack_free (jc);
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
	std::string const other_shrt = _engine->make_port_name_non_relative (other);
	std::string const this_shrt = _engine->make_port_name_non_relative (_name);

	int r = 0;

	if (sends_output ()) {
		r = jack_disconnect (_engine->jack (), this_shrt.c_str (), other_shrt.c_str ());
	} else {
		r = jack_disconnect (_engine->jack (), other_shrt.c_str (), this_shrt.c_str ());
	}

	if (r == 0) {
		_connections.erase (other);
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
Port::ensure_monitor_input (bool yn)
{
	jack_port_ensure_monitor (_jack_port, yn);
}

bool
Port::monitoring_input () const
{
	return jack_port_monitoring_input (_jack_port);
}

void
Port::reset ()
{
	_last_monitor = false;

	// XXX
	// _metering = 0;
	// reset_meters ();
}

void
Port::recompute_total_latency () const
{
#ifndef HAVE_JACK_NEW_LATENCY
#ifdef  HAVE_JACK_RECOMPUTE_LATENCY
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		return;
	}

	jack_recompute_total_latency (jack, _jack_port);
#endif
#endif
}

#ifdef HAVE_JACK_NEW_LATENCY
void
Port::set_latency_range (jack_latency_range_t& range, bool playback) const
{
        if (!jack_port_set_latency_range) {
                return;
        }

        jack_port_set_latency_range (_jack_port, (playback ? JackPlaybackLatency : JackCaptureLatency), &range);
}
#endif

#ifdef HAVE_JACK_NEW_LATENCY
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

                for (vector<string>::iterator c = connections.begin(); c != connections.end(); ++c) {
                        jack_port_t* remote_port = jack_port_by_name (_engine->jack(), (*c).c_str());
                        jack_latency_range_t lr;

                        DEBUG_TRACE (DEBUG::Latency, string_compose ("\t%1 connected to %2\n", name(), *c));

                        if (remote_port) {
                                jack_port_get_latency_range (remote_port, (playback ? JackPlaybackLatency : JackCaptureLatency), &lr);
                                DEBUG_TRACE (DEBUG::Latency, string_compose ("\t\tremote has latency range %1 .. %2\n", lr.min, lr.max));
                                range.min = min (range.min, lr.min);
                                range.max = max (range.max, lr.max);
                        }
                }

        } else {

                range.min = 0;
                range.max = 0;
        }
}
#endif /* HAVE_JACK_NEW_LATENCY */

framecnt_t
Port::total_latency () const
{
#ifndef HAVE_JACK_NEW_LATENCY
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		return 0;
	}

	return jack_port_get_total_latency (jack, _jack_port);
#else
        return 0;
#endif
}

int
Port::reestablish ()
{
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		return -1;
	}

	cerr << "RE-REGISTER: " << _name.c_str() << endl;
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
		_name = n;
	}

	return r;
}

void
Port::request_monitor_input (bool yn)
{
	jack_port_request_monitor (_jack_port, yn);
}

void
Port::set_latency (framecnt_t n)
{
#ifndef HAVE_JACK_NEW_LATENCY
	jack_port_set_latency (_jack_port, n);
#endif
}

bool
Port::physically_connected () const
{
	const char** jc = jack_port_get_connections (_jack_port);

	if (jc) {
		for (int i = 0; jc[i]; ++i) {

                        jack_port_t* port = jack_port_by_name (_engine->jack(), jc[i]);
                        
                        if (port && (jack_port_flags (port) & JackPortIsPhysical)) {
                                jack_free (jc);
                                return true;
                        }
		}
                
		jack_free (jc);
	}

        return false;
}

