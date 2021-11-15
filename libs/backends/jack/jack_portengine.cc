/*
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "pbd/error.h"

#include "jack_audiobackend.h"
#include "jack_connection.h"

#include "ardour/port_manager.h"

#include "pbd/i18n.h"

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

void
JACKAudioBackend::when_connected_to_jack ()
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

int
JACKAudioBackend::set_port_name (PortHandle port, const std::string& name)
{
#if HAVE_JACK_PORT_RENAME
	jack_client_t* client = _jack_connection->jack();
	if (client) {
		return jack_port_rename (client, boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, name.c_str());
	} else {
		return -1;
	}
#else
	return jack_port_set_name (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, name.c_str());
#endif
}

string
JACKAudioBackend::get_port_name (PortHandle port) const
{
	return jack_port_name (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr);
}

PortFlags
JACKAudioBackend::get_port_flags (PortHandle port) const
{
	return PortFlags (jack_port_flags (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr));
}

int
JACKAudioBackend::get_port_property (PortHandle port, const std::string& key, std::string& value, std::string& type) const
{
#ifdef HAVE_JACK_METADATA // really everyone ought to have this by now.
	int rv = -1;
	char *cvalue = NULL;
	char *ctype = NULL;

	jack_uuid_t uuid = jack_port_uuid (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr);
	rv = jack_get_property (uuid, key.c_str(), &cvalue, &ctype);

	if (0 == rv && cvalue) {
		value = cvalue;
		if (ctype) {
			type = ctype;
		}
	} else {
		rv = -1;
	}

	jack_free(cvalue);
	jack_free(ctype);
	return rv;
#else
	return -1;
#endif
}

int
JACKAudioBackend::set_port_property (PortHandle port, const std::string& key, const std::string& value, const std::string& type)
{
#ifdef HAVE_JACK_METADATA // really everyone ought to have this by now.
	int rv = -1;
	jack_client_t* client = _jack_connection->jack();
	jack_uuid_t uuid = jack_port_uuid (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr);
	return jack_set_property(client, uuid, key.c_str(), value.c_str(), type.c_str());
	return rv;
#else
	return -1;
#endif
}

PortEngine::PortPtr
JACKAudioBackend::get_port_by_name (const std::string& name) const
{
	{
		boost::shared_ptr<JackPorts> ports = _jack_ports.reader ();
		JackPorts::iterator p = ports->find (name);

		if (p != ports->end()) {
			return p->second;
		}
	}

	/* Port not known to us yet, so look it up via JACK (slow) */

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, PortEngine::PortPtr());
	jack_port_t * jack_port = jack_port_by_name (_priv_jack, name.c_str());

	if (!jack_port) {
		/* No such port ... return nothering */
		return PortEngine::PortPtr();
	}

	boost::shared_ptr<JackPort> jp;

	{
		RCUWriter<JackPorts> writer (_jack_ports);
		boost::shared_ptr<JackPorts> ports = writer.get_copy();
		jp.reset (new JackPort (jack_port));
		ports->insert (std::make_pair (name, jp));
	}

	_jack_ports.flush ();

	return jp;
}

void
JACKAudioBackend::_registration_callback (jack_port_id_t id, int reg, void* arg)
{
	/* we don't use a virtual method for the registration callback, because
	   JACK is the only backend that delivers the arguments shown above. So
	   call our own JACK-centric registration callback, then the generic
	   one.
	*/
	static_cast<JACKAudioBackend*> (arg)->jack_registration_callback (id, reg);
	static_cast<JACKAudioBackend*> (arg)->manager.registration_callback ();
	static_cast<JACKAudioBackend*> (arg)->engine.latency_callback (false);
	static_cast<JACKAudioBackend*> (arg)->engine.latency_callback (true);
}

void
JACKAudioBackend::jack_registration_callback (jack_port_id_t id, int reg)
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	jack_port_t* jack_port = jack_port_by_id (_priv_jack, id);

	if (!jack_port) {
		return;
	}

	/* We only need to care about ports that we do not register/unregister
	 * ourselves. Our own ports will added/removed from _jack_ports at the
	 * appropriate time.
	 *
	 * But for the input meters, we'll be looking up ports not created by
	 * us, and they may also go away at arbitrary times too. We want to
	 * make sure we can look up these ports by name only (in _jack_ports)
	 * because jack_port_by_name() is unacceptably slow for RT contexts
	 * (like run_input_meters()). So we catch these ports at registration
	 * time, and put a suitable entry in _jack_ports.
	 *
	 * It isn't critical that we keep _jack_ports current if any of these
	 * ports goes away, but since we get told about that here, we do that
	 * just to keep things clean. This will happen if someone disconnects a
	 * USB MIDI device, for example.
	 */

	if (!jack_port_is_mine (_priv_jack, jack_port)) {

		const char* name = jack_port_name (jack_port);

		boost::shared_ptr<JackPorts> ports = _jack_ports.write_copy();

		if (!reg) {
			if (ports->erase (name)) {
				_jack_ports.update (ports);
			} else {
				_jack_ports.no_update();
			}
		} else {
			if (ports->find (name) != ports->end()) {
				/* hmmm, we already have this port */
				std::cout << "re-registration of JACK port named " << name << std::endl;
				ports->erase (name);
			}

			boost::shared_ptr<JackPort> jp (new JackPort (jack_port));
			ports->insert (std::make_pair (name, jp));

			_jack_ports.update (ports);
		}
	}
}

int
JACKAudioBackend::_graph_order_callback (void *arg)
{
	return static_cast<JACKAudioBackend*> (arg)->manager.graph_order_callback ();
}

void
JACKAudioBackend::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	static_cast<JACKAudioBackend*> (arg)->connect_callback (id_a, id_b, conn);
}

void
JACKAudioBackend::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
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
JACKAudioBackend::connected (PortHandle p, bool process_callback_safe)
{
	bool ret = false;
	jack_port_t* port = boost::dynamic_pointer_cast<JackPort>(p)->jack_ptr;
	const char** ports;

	if (process_callback_safe) {
		ports = jack_port_get_connections (port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
		ports = jack_port_get_all_connections (_priv_jack, port);
	}

	if (ports) {
		ret = true;
	}

	jack_free (ports);

	return ret;
}

bool
JACKAudioBackend::connected_to (PortHandle p, const std::string& other, bool process_callback_safe)
{
	bool ret = false;
	const char** ports;
	jack_port_t* port = boost::dynamic_pointer_cast<JackPort>(p)->jack_ptr;

	if (process_callback_safe) {
		ports = jack_port_get_connections (port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
		ports = jack_port_get_all_connections (_priv_jack, port);
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
JACKAudioBackend::physically_connected (PortHandle p, bool process_callback_safe)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
	jack_port_t* port = boost::dynamic_pointer_cast<JackPort>(p)->jack_ptr;

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
				jack_free (ports);
				return true;
			}
		}
		jack_free (ports);
	}

	return false;
}

bool
JACKAudioBackend::externally_connected (PortHandle p, bool process_callback_safe)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);
	jack_port_t* port = boost::dynamic_pointer_cast<JackPort>(p)->jack_ptr;

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
				jack_free (ports);
				return true;
			}
			if (other && !jack_port_is_mine (_priv_jack, other)) {
				jack_free (ports);
				return true;
			}
		}
		jack_free (ports);
	}
	return false;
}

int
JACKAudioBackend::get_connections (PortHandle p, vector<string>& s, bool process_callback_safe)
{
	const char** ports;
	jack_port_t* port = boost::dynamic_pointer_cast<JackPort>(p)->jack_ptr;

	if (process_callback_safe) {
		ports = jack_port_get_connections (port);
	} else {
		GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
		ports = jack_port_get_all_connections (_priv_jack, port);
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
JACKAudioBackend::port_data_type (PortHandle port) const
{
	return jack_port_type_to_ardour_data_type (jack_port_type (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr));
}

const string&
JACKAudioBackend::my_name() const
{
	return _jack_connection->client_name();
}

bool
JACKAudioBackend::port_is_physical (PortHandle port) const
{
	if (!port) {
                return false;
        }

	return jack_port_flags (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr) & JackPortIsPhysical;
}

int
JACKAudioBackend::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s) const
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
JACKAudioBackend::n_physical_inputs () const
{
	return n_physical (JackPortIsInput);
}

ChanCount
JACKAudioBackend::n_physical_outputs () const
{
	return n_physical (JackPortIsOutput);
}

void
JACKAudioBackend::get_physical (DataType type, unsigned long flags, vector<string>& phy) const
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
JACKAudioBackend::get_physical_inputs (DataType type, vector<string>& ins)
{
	get_physical (type, JackPortIsOutput, ins);
}

/** Get physical ports for which JackPortIsInput is set; ie those that correspond to
 *  a physical output connector.
 */
void
JACKAudioBackend::get_physical_outputs (DataType type, vector<string>& outs)
{
	get_physical (type, JackPortIsInput, outs);
}


bool
JACKAudioBackend::can_monitor_input () const
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
JACKAudioBackend::request_input_monitoring (PortHandle port, bool yn)
{
	return jack_port_request_monitor (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, yn);
}
int
JACKAudioBackend::ensure_input_monitoring (PortHandle port, bool yn)
{
	return jack_port_ensure_monitor (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, yn);
}
bool
JACKAudioBackend::monitoring_input (PortHandle port)
{
	return jack_port_monitoring_input (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr);
}

PortEngine::PortPtr
JACKAudioBackend::register_port (const std::string& shortname, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, PortEngine::PortPtr());
	jack_port_t* jack_port = jack_port_register (_priv_jack, shortname.c_str(),
	                                             ardour_data_type_to_jack_port_type (type),
	                                             ardour_port_flags_to_jack_flags (flags),
	                                             0);
	if (!jack_port) {
		return PortEngine::PortPtr();
	}

	boost::shared_ptr<JackPort> jp;

	{
		RCUWriter<JackPorts> writer (_jack_ports);
		boost::shared_ptr<JackPorts> ports = writer.get_copy();

		jp.reset (new JackPort (jack_port));

		ports->insert (std::make_pair (jack_port_name (jack_port), jp));
	}

	_jack_ports.flush();

	 return jp;
}

void
JACKAudioBackend::unregister_port (PortHandle port)
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	boost::shared_ptr<JackPort> jp = boost::dynamic_pointer_cast<JackPort>(port);
	const std::string name = jack_port_name (jp->jack_ptr);

	{
		RCUWriter<JackPorts> writer (_jack_ports);
		boost::shared_ptr<JackPorts> ports = writer.get_copy();
		ports->erase (name);
	}

	_jack_ports.flush ();

	(void) jack_port_unregister (_priv_jack, jp->jack_ptr);
}

int
JACKAudioBackend::connect (PortHandle port, const std::string& other)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	int r = jack_connect (_priv_jack, jack_port_name (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr), other.c_str());
	if (r == 0 || r == EEXIST) {
		return 0;
	}
	return r;
}
int
JACKAudioBackend::connect (const std::string& src, const std::string& dst)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);

	int r = jack_connect (_priv_jack, src.c_str(), dst.c_str());
	if (r == 0 || r == EEXIST) {
		return 0;
	}
	return r;
}

int
JACKAudioBackend::disconnect (PortHandle port, const std::string& other)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_disconnect (_priv_jack, jack_port_name (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr), other.c_str());
}

int
JACKAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_disconnect (_priv_jack, src.c_str(), dst.c_str());
}

int
JACKAudioBackend::disconnect_all (PortHandle port)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	return jack_port_disconnect (_priv_jack, boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr);
}

int
JACKAudioBackend::midi_event_get (pframes_t& timestamp, size_t& size, uint8_t const** buf, void* port_buffer, uint32_t event_index)
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
JACKAudioBackend::midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size)
{
	return jack_midi_event_write (port_buffer, timestamp, buffer, size);
}

uint32_t
JACKAudioBackend::get_midi_event_count (void* port_buffer)
{
	return jack_midi_get_event_count (port_buffer);
}

void
JACKAudioBackend::midi_clear (void* port_buffer)
{
	jack_midi_clear_buffer (port_buffer);
}

void
JACKAudioBackend::set_latency_range (PortHandle port, bool for_playback, LatencyRange r)
{
	jack_latency_range_t range;

	range.min = r.min;
	range.max = r.max;

	jack_port_set_latency_range (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, for_playback ? JackPlaybackLatency : JackCaptureLatency, &range);
}

LatencyRange
JACKAudioBackend::get_latency_range (PortHandle port, bool for_playback)
{
	jack_latency_range_t range;
	LatencyRange ret;

	jack_port_get_latency_range (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, for_playback ? JackPlaybackLatency : JackCaptureLatency, &range);

	ret.min = range.min;
	ret.max = range.max;

	return ret;
}

void*
JACKAudioBackend::get_buffer (PortHandle port, pframes_t nframes)
{
	return jack_port_get_buffer (boost::dynamic_pointer_cast<JackPort>(port)->jack_ptr, nframes);
}

uint32_t
JACKAudioBackend::port_name_size() const
{
	return jack_port_name_size ();
}
