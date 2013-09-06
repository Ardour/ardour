/*
    Copyright (C) 2013 Paul Davis

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

#include <string.h>
#include <stdint.h>

#include "pbd/error.h"

#include "ardour/jack_portengine.h"
#include "ardour/jack_connection.h"
#include "ardour/port_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;
using std::vector;

#define GET_PRIVATE_JACK_POINTER(localvar)  jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(localvar,r) jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return r; }

static uint32_t
ardour_port_flags_to_jack_flags (PortFlags flags)
{
	uint32_t jack_flags = 0;
	
	if (flags & IsInput) {
		jack_flags |= JackPortIsInput;
	}
	if (flags & IsOutput) {
		jack_flags |= JackPortIsOutput;
	}
	if (flags & IsTerminal) {
		jack_flags |= JackPortIsTerminal;
	}
	if (flags & IsPhysical) {
		jack_flags |= JackPortIsPhysical;
	}
	if (flags & CanMonitor) {
		jack_flags |= JackPortCanMonitor;
	}

	return jack_flags;
}

static DataType
jack_port_type_to_ardour_data_type (const char* jack_type)
{
	if (strcmp (jack_type, JACK_DEFAULT_AUDIO_TYPE) == 0) {
		return DataType::AUDIO;
	} else if (strcmp (jack_type, JACK_DEFAULT_MIDI_TYPE) == 0) {
		return DataType::MIDI;
	}
	return DataType::NIL;
}

static const char*
ardour_data_type_to_jack_port_type (DataType d)
{
	switch (d) {
	case DataType::AUDIO:
		return JACK_DEFAULT_AUDIO_TYPE;
	case DataType::MIDI:
		return JACK_DEFAULT_MIDI_TYPE;
	}

	return "";
}

JACKPortEngine::JACKPortEngine (PortManager& pm, boost::shared_ptr<JackConnection> jc)
	: PortEngine (pm)
	, _jack_connection (jc)
{
	_jack_connection->Connected.connect_same_thread (jack_connection_connection, boost::bind (&JACKPortEngine::connected_to_jack, this));
}

JACKPortEngine::~JACKPortEngine ()
{
	/* a default destructor would do this, and so would this one,
	   but we'll make it explicit in case we ever need to debug
	   the lifetime of the JACKConnection
	*/
	_jack_connection.reset ();
}

void
JACKPortEngine::connected_to_jack ()
{
	/* register callbacks for stuff that is our responsibility */

	jack_client_t* client = _jack_connection->jack();

	if (!client) {
		/* how could this happen? it could ... */
		error << _("Already disconnected from JACK before PortEngine could register callbacks") << endmsg;
		return;
	}

        jack_set_port_registration_callback (client, _registration_callback, this);
        jack_set_port_connect_callback (client, _connect_callback, this);
        jack_set_graph_order_callback (client, _graph_order_callback, this);
}

void*
JACKPortEngine::private_handle() const
{
	return _jack_connection->jack();
}

bool
JACKPortEngine::connected() const
{
	return _jack_connection->connected();
}

int
JACKPortEngine::set_port_name (PortHandle port, const std::string& name)
{
	return jack_port_set_name ((jack_port_t*) port, name.c_str());
}

string
JACKPortEngine::get_port_name (PortHandle port) const
{
	return jack_port_name ((jack_port_t*) port);
}

PortEngine::PortHandle*
JACKPortEngine:: get_port_by_name (const std::string& name) const
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return (PortHandle*) jack_port_by_name (_priv_jack, name.c_str());
}

void
JACKPortEngine::_registration_callback (jack_port_id_t /*id*/, int /*reg*/, void* arg)
{
	static_cast<JACKPortEngine*> (arg)->manager.registration_callback ();
}

int
JACKPortEngine::_graph_order_callback (void *arg)
{
	return static_cast<JACKPortEngine*> (arg)->manager.graph_order_callback ();
}

void
JACKPortEngine::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	static_cast<JACKPortEngine*> (arg)->connect_callback (id_a, id_b, conn);
}

void
JACKPortEngine::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
{
	if (manager.port_remove_in_progress()) {
		return;
	}

	GET_PRIVATE_JACK_POINTER (_priv_jack);

	jack_port_t* a = jack_port_by_id (_priv_jack, id_a);
	jack_port_t* b = jack_port_by_id (_priv_jack, id_b);

	manager.connect_callback (jack_port_name (a), jack_port_name (b), conn == 0 ? false : true);
}

bool
JACKPortEngine::connected (PortHandle port, bool process_callback_safe)
{
	bool ret = false;

	const char** ports;

	if (process_callback_safe) {
		ports = jack_port_get_connections ((jack_port_t*)port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
		ports = jack_port_get_all_connections (_priv_jack, (jack_port_t*)port);
	}

	if (ports) {
		ret = true;
	}

	jack_free (ports);

	return ret;
}

bool
JACKPortEngine::connected_to (PortHandle port, const std::string& other, bool process_callback_safe)
{
	bool ret = false;
	const char** ports;

	if (process_callback_safe) {
		ports = jack_port_get_connections ((jack_port_t*)port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
		ports = jack_port_get_all_connections (_priv_jack, (jack_port_t*)port);
	}

	if (ports) {
		for (int i = 0; ports[i]; ++i) {
			if (other == ports[i]) {
				ret = true;
			}
		}
		jack_free (ports);
	}

	return ret;
}

bool
JACKPortEngine::physically_connected (PortHandle p, bool process_callback_safe)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
	jack_port_t* port = (jack_port_t*) p;

	const char** ports;
	
	if (process_callback_safe) {
		ports = jack_port_get_connections ((jack_port_t*)port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
		ports = jack_port_get_all_connections (_priv_jack, (jack_port_t*)port);
	}

	if (ports) {
		for (int i = 0; ports[i]; ++i) {

			jack_port_t* other = jack_port_by_name (_priv_jack, ports[i]);

			if (other && (jack_port_flags (other) & JackPortIsPhysical)) {
				return true;
			}
		}
		jack_free (ports);
	}

	return false;
}

int
JACKPortEngine::get_connections (PortHandle port, vector<string>& s, bool process_callback_safe)
{
	const char** ports;

	if (process_callback_safe) {
		ports = jack_port_get_connections ((jack_port_t*)port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
		ports = jack_port_get_all_connections (_priv_jack, (jack_port_t*)port);
	}

	if (ports) {
		for (int i = 0; ports[i]; ++i) {
			s.push_back (ports[i]);
		}
		jack_free (ports);
	}

	return s.size();
}

DataType
JACKPortEngine::port_data_type (PortHandle p) const
{
	return jack_port_type_to_ardour_data_type (jack_port_type ((jack_port_t*) p));
}

const string&
JACKPortEngine::my_name() const
{
	return _jack_connection->client_name();
}

bool
JACKPortEngine::port_is_physical (PortHandle ph) const
{
	if (!ph) {
                return false;
        }

        return jack_port_flags ((jack_port_t*) ph) & JackPortIsPhysical;
}

int
JACKPortEngine::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s) const
{

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack,0);

	const char** ports =  jack_get_ports (_priv_jack, port_name_pattern.c_str(), 
					      ardour_data_type_to_jack_port_type (type), 
					      ardour_port_flags_to_jack_flags (flags));

	if (ports == 0) {
		return 0;
	}

	for (uint32_t i = 0; ports[i]; ++i) {
		s.push_back (ports[i]);
	}

	jack_free (ports);
	
	return s.size();
}

ChanCount
JACKPortEngine::n_physical (unsigned long flags) const
{
	ChanCount c;

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, c);

	const char ** ports = jack_get_ports (_priv_jack, NULL, NULL, JackPortIsPhysical | flags);

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
			if (!strstr (ports[i], "Midi-Through")) {
				DataType t = port_data_type (jack_port_by_name (_priv_jack, ports[i]));
				if (t != DataType::NIL) {
					c.set (t, c.get (t) + 1);
				}
			}
		}
		
		jack_free (ports);
	}

	return c;
}

ChanCount
JACKPortEngine::n_physical_inputs () const
{
	return n_physical (JackPortIsInput);
}

ChanCount
JACKPortEngine::n_physical_outputs () const
{
	return n_physical (JackPortIsOutput);
}

void
JACKPortEngine::get_physical (DataType type, unsigned long flags, vector<string>& phy) const
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, ardour_data_type_to_jack_port_type (type), JackPortIsPhysical | flags)) == 0) {
		return;
	}

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
                        if (strstr (ports[i], "Midi-Through")) {
                                continue;
                        }
			phy.push_back (ports[i]);
		}
		jack_free (ports);
	}
}

/** Get physical ports for which JackPortIsOutput is set; ie those that correspond to
 *  a physical input connector.
 */
void
JACKPortEngine::get_physical_inputs (DataType type, vector<string>& ins)
{
	get_physical (type, JackPortIsOutput, ins);
}

/** Get physical ports for which JackPortIsInput is set; ie those that correspond to
 *  a physical output connector.
 */
void
JACKPortEngine::get_physical_outputs (DataType type, vector<string>& outs)
{
	get_physical (type, JackPortIsInput, outs);
}


bool
JACKPortEngine::can_monitor_input () const
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack,false);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortCanMonitor)) == 0) {
		return false;
	}

	jack_free (ports);

	return true;
}

int
JACKPortEngine::request_input_monitoring (PortHandle port, bool yn)
{
	return jack_port_request_monitor ((jack_port_t*) port, yn);
}
int
JACKPortEngine::ensure_input_monitoring (PortHandle port, bool yn)
{
	return jack_port_ensure_monitor ((jack_port_t*) port, yn);
}
bool
JACKPortEngine::monitoring_input (PortHandle port)
{
	return jack_port_monitoring_input ((jack_port_t*) port);
}


pframes_t
JACKPortEngine::sample_time_at_cycle_start ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_last_frame_time (_priv_jack);
}


PortEngine::PortHandle
JACKPortEngine::register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_port_register (_priv_jack, shortname.c_str(), 
				   ardour_data_type_to_jack_port_type (type),
				   ardour_port_flags_to_jack_flags (flags),
				   0);
}

void
JACKPortEngine::unregister_port (PortHandle port)
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	(void) jack_port_unregister (_priv_jack, (jack_port_t*) port);
}

int
JACKPortEngine::connect (PortHandle port, const std::string& other)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_connect (_priv_jack, jack_port_name ((jack_port_t*) port), other.c_str());
}
int
JACKPortEngine::connect (const std::string& src, const std::string& dst)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	
	int r = jack_connect (_priv_jack, src.c_str(), dst.c_str());
	return r;
}

int
JACKPortEngine::disconnect (PortHandle port, const std::string& other)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_disconnect (_priv_jack, jack_port_name ((jack_port_t*) port), other.c_str());
}

int
JACKPortEngine::disconnect (const std::string& src, const std::string& dst)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_disconnect (_priv_jack, src.c_str(), dst.c_str());
}

int
JACKPortEngine::disconnect_all (PortHandle port)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_port_disconnect (_priv_jack, (jack_port_t*) port);
}

int
JACKPortEngine::midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index)
{
	jack_midi_event_t ev;
	int ret;

	if ((ret = jack_midi_event_get (&ev, port_buffer, event_index)) == 0) {
		timestamp = ev.time;
		size = ev.size;
		*buf = ev.buffer;
	}

	return ret;
}

int
JACKPortEngine::midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size)
{
	return jack_midi_event_write (port_buffer, timestamp, buffer, size);
}

uint32_t
JACKPortEngine::get_midi_event_count (void* port_buffer)
{
	return jack_midi_get_event_count (port_buffer);
}

void
JACKPortEngine::midi_clear (void* port_buffer)
{
	jack_midi_clear_buffer (port_buffer);
}

void
JACKPortEngine::set_latency_range (PortHandle port, bool for_playback, LatencyRange r)
{
	jack_latency_range_t range;
	
	range.min = r.min;
	range.max = r.max;

	jack_port_set_latency_range ((jack_port_t*) port, for_playback ? JackPlaybackLatency : JackCaptureLatency, &range);
}

LatencyRange
JACKPortEngine::get_latency_range (PortHandle port, bool for_playback)
{
	jack_latency_range_t range;
	LatencyRange ret;
	
	jack_port_get_latency_range ((jack_port_t*) port, for_playback ? JackPlaybackLatency : JackCaptureLatency, &range);

	ret.min = range.min;
	ret.max = range.max;

	return ret;
}

void*
JACKPortEngine::get_buffer (PortHandle port, pframes_t nframes)
{
	return jack_port_get_buffer ((jack_port_t*) port, nframes);
}

uint32_t
JACKPortEngine::port_name_size() const
{
	return jack_port_name_size ();
}
