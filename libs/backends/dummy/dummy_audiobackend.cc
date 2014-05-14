/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/time.h>
#include <regex.h>

#include <glibmm.h>

#include "dummy_audiobackend.h"
#include "pbd/error.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t DummyAudioBackend::_max_buffer_size = 8192;

DummyAudioBackend::DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _running (false)
	, _freewheeling (false)
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _dsp_load (0)
	, _n_inputs (0)
	, _n_outputs (0)
	, _systemic_input_latency (0)
	, _systemic_output_latency (0)
	, _processed_samples (0)
{
	_instance_name = s_instance_name;
}

DummyAudioBackend::~DummyAudioBackend ()
{
}

/* AUDIOBACKEND API */

std::string
DummyAudioBackend::name () const
{
	return X_("Dummy");
}

bool
DummyAudioBackend::is_realtime () const
{
	return false;
}

std::vector<AudioBackend::DeviceStatus>
DummyAudioBackend::enumerate_devices () const
{
	std::vector<AudioBackend::DeviceStatus> s;
	s.push_back (DeviceStatus (_("Dummy"), true));
	return s;
}

std::vector<float>
DummyAudioBackend::available_sample_rates (const std::string&) const
{
	std::vector<float> sr;
	sr.push_back (8000.0);
	sr.push_back (22050.0);
	sr.push_back (24000.0);
	sr.push_back (44100.0);
	sr.push_back (48000.0);
	sr.push_back (88200.0);
	sr.push_back (96000.0);
	sr.push_back (176400.0);
	sr.push_back (192000.0);
	return sr;
}

std::vector<uint32_t>
DummyAudioBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
	bs.push_back (4);
	bs.push_back (8);
	bs.push_back (16);
	bs.push_back (32);
	bs.push_back (64);
	bs.push_back (128);
	bs.push_back (256);
	bs.push_back (512);
	bs.push_back (1024);
	bs.push_back (2048);
	bs.push_back (4096);
	bs.push_back (8192);
	return bs;
}

uint32_t
DummyAudioBackend::available_input_channel_count (const std::string&) const
{
	return 128;
}

uint32_t
DummyAudioBackend::available_output_channel_count (const std::string&) const
{
	return 128;
}

bool
DummyAudioBackend::can_change_sample_rate_when_running () const
{
	return true;
}

bool
DummyAudioBackend::can_change_buffer_size_when_running () const
{
	return true;
}

int
DummyAudioBackend::set_device_name (const std::string&)
{
	return 0;
}

int
DummyAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) { return -1; }
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
DummyAudioBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (bs);
	return 0;
}

int
DummyAudioBackend::set_interleaved (bool yn)
{
	if (!yn) { return 0; }
	return -1;
}

int
DummyAudioBackend::set_input_channels (uint32_t cc)
{
	_n_inputs = cc;
	return 0;
}

int
DummyAudioBackend::set_output_channels (uint32_t cc)
{
	_n_outputs = cc;
	return 0;
}

int
DummyAudioBackend::set_systemic_input_latency (uint32_t sl)
{
	_systemic_input_latency = sl;
	return 0;
}

int
DummyAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	_systemic_output_latency = sl;
	return 0;
}

/* Retrieving parameters */
std::string
DummyAudioBackend::device_name () const
{
	return _("Dummy Device");
}

float
DummyAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
DummyAudioBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
DummyAudioBackend::interleaved () const
{
	return false;
}

uint32_t
DummyAudioBackend::input_channels () const
{
	return _n_inputs;
}

uint32_t
DummyAudioBackend::output_channels () const
{
	return _n_outputs;
}

uint32_t
DummyAudioBackend::systemic_input_latency () const
{
	return _systemic_input_latency;
}

uint32_t
DummyAudioBackend::systemic_output_latency () const
{
	return _systemic_output_latency;
}

/* MIDI */
std::vector<std::string>
DummyAudioBackend::enumerate_midi_options () const
{
	std::vector<std::string> m;
	m.push_back (_("None"));
	return m;
}

int
DummyAudioBackend::set_midi_option (const std::string&)
{
	return -1;
}

std::string
DummyAudioBackend::midi_option () const
{
	return "";
}

/* State Control */

static void * pthread_process (void *arg)
{
	DummyAudioBackend *d = static_cast<DummyAudioBackend *>(arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
DummyAudioBackend::_start (bool /*for_latency_measurement*/)
{
	if (_running) {
		PBD::error << _("DummyAudioBackend: already active.") << endmsg;
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("DummyAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_ports.clear();
	}

	if (register_system_ports()) {
		PBD::error << _("DummyAudioBackend: failed to register system ports.") << endmsg;
		return -1;
	}

	if (engine.reestablish_ports ()) {
		PBD::error << _("DummyAudioBackend: Could not re-establish ports.") << endmsg;
		stop ();
		return -1;
	}

	engine.reconnect_ports ();

	if (pthread_create (&_main_thread, NULL, pthread_process, this)) {
		PBD::error << _("DummyAudioBackend: cannot start.") << endmsg;
	}

	int timeout = 5000;
	while (!_running && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_running) {
		PBD::error << _("DummyAudioBackend: failed to start process thread.") << endmsg;
		return -1;
	}

	return 0;
}

int
DummyAudioBackend::stop ()
{
	void *status;
	if (!_running) {
		return -1;
	}

	_running = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("DummyAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}
	unregister_system_ports();
	return 0;
}

int
DummyAudioBackend::freewheel (bool onoff)
{
	if (onoff == _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;
	engine.freewheel_callback (onoff);
	return 0;
}

float
DummyAudioBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
DummyAudioBackend::raw_buffer_size (DataType t)
{
	switch (t) {
		case DataType::AUDIO:
			return _max_buffer_size * sizeof(Sample);
		case DataType::MIDI:
			return _max_buffer_size; // XXX not really limited
	}
	return 0;
}

/* Process time */
pframes_t
DummyAudioBackend::sample_time ()
{
	return _processed_samples;
}

pframes_t
DummyAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
DummyAudioBackend::samples_since_cycle_start ()
{
	return 0;
}


void *
DummyAudioBackend::dummy_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
DummyAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t thread_id;
	pthread_attr_t attr;
	size_t stacksize = 100000;

	pthread_attr_init (&attr);
	pthread_attr_setstacksize (&attr, stacksize);
	ThreadData* td = new ThreadData (this, func, stacksize);

	if (pthread_create (&thread_id, &attr, dummy_process_thread, td)) {
		PBD::error << _("AudioEngine: cannot create process thread.") << endmsg;
		return -1;
	}

	_threads.push_back (thread_id);
	return 0;
}

int
DummyAudioBackend::join_process_threads ()
{
	int rv = 0;

	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i)
	{
		void *status;
		if (pthread_join (*i, &status)) {
			PBD::error << _("AudioEngine: cannot terminate process thread.") << endmsg;
			rv -= 1;
		}
	}
	_threads.clear ();
	return rv;
}

bool
DummyAudioBackend::in_process_thread ()
{
	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i)
	{
		if (pthread_equal (*i, pthread_self ()) != 0) {
			return true;
		}
	}
	return false;
}

uint32_t
DummyAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
DummyAudioBackend::update_latencies ()
{
}

/* PORTENGINE API */

void*
DummyAudioBackend::private_handle () const
{
	return NULL;
}

const std::string&
DummyAudioBackend::my_name () const
{
	return _instance_name;
}

bool
DummyAudioBackend::available () const
{
	return true;
}

uint32_t
DummyAudioBackend::port_name_size () const
{
	return 256;
}

int
DummyAudioBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<DummyPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
DummyAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<DummyPort*>(port)->name ();
}

PortEngine::PortHandle
DummyAudioBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle) find_port (name);
	return port;
}

int
DummyAudioBackend::get_ports (
		const std::string& port_name_pattern,
		DataType type, PortFlags flags,
		std::vector<std::string>& port_names) const
{
	int rv = 0;
	regex_t port_regex;
	bool use_regexp = false;
	if (port_name_pattern.size () > 0) {
		if (!regcomp (&port_regex, port_name_pattern.c_str (), REG_EXTENDED|REG_NOSUB)) {
			use_regexp = true;
		}
	}
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if ((port->type () == type) && (port->flags () & flags)) {
			if (!use_regexp || !regexec (&port_regex, port->name ().c_str (), 0, NULL, 0)) {
				port_names.push_back (port->name ());
				++rv;
			}
		}
	}
	if (use_regexp) {
		regfree (&port_regex);
	}
	return rv;
}

DataType
DummyAudioBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<DummyPort*>(port)->type ();
}

PortEngine::PortHandle
DummyAudioBackend::register_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
DummyAudioBackend::add_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("DummyBackend::register_port: Port already exists:")
				<< " (" << name << ")" << endmsg;
		return 0;
	}
	DummyPort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new DummyAudioPort (name, flags);
			break;
		case DataType::MIDI:
			port = new DummyMidiPort (name, flags);
			break;
		default:
			PBD::error << _("DummyBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.push_back (port);

	return port;
}

void
DummyAudioBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!valid_port (port_handle)) {
		PBD::error << _("DummyBackend::unregister_port: Invalid Port.") << endmsg;
	}
	DummyPort* port = static_cast<DummyPort*>(port_handle);
	std::vector<DummyPort*>::iterator i = std::find (_ports.begin (), _ports.end (), static_cast<DummyPort*>(port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("DummyBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all(port_handle);
	_ports.erase (i);
	delete port;
}

int
DummyAudioBackend::register_system_ports()
{
	LatencyRange lr;

	const int a_ins = _n_inputs > 0 ? _n_inputs : 8;
	const int a_out = _n_outputs > 0 ? _n_outputs : 8;
	const int m_ins = 2; // TODO
	const int m_out = 2;

	/* audio ports */
	lr.min = lr.max = _samples_per_period + _systemic_input_latency;
	for (int i = 1; i <= a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
	}

	lr.min = lr.max = _samples_per_period + _systemic_output_latency;
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
	}

	/* midi ports */
	lr.min = lr.max = _samples_per_period + _systemic_input_latency;
	for (int i = 1; i <= m_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_capture_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
	}

	lr.min = lr.max = _samples_per_period + _systemic_output_latency;
	for (int i = 1; i <= m_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
	}

	return 0;
}

void
DummyAudioBackend::unregister_system_ports()
{
	size_t i = 0;
	while (i <  _ports.size ()) {
		DummyPort* port = _ports[i];
		if (port->is_physical () && port->is_terminal ()) {
			port->disconnect_all ();
			_ports.erase (_ports.begin() + i);
		} else {
			++i;
		}
	}
}

int
DummyAudioBackend::connect (const std::string& src, const std::string& dst)
{
	DummyPort* src_port = find_port (src);
	DummyPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("DummyBackend::connect: Invalid Source port:")
				<< " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("DummyBackend::connect: Invalid Destination port:")
			<< " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
DummyAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	DummyPort* src_port = find_port (src);
	DummyPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("DummyBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
DummyAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	DummyPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("DummyBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("DummyBackend::connect: Invalid Destination Port")
			<< " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<DummyPort*>(src)->connect (dst_port);
}

int
DummyAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	DummyPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("DummyBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<DummyPort*>(src)->disconnect (dst_port);
}

int
DummyAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<DummyPort*>(port)->disconnect_all ();
	return 0;
}

bool
DummyAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<DummyPort*>(port)->is_connected ();
}

bool
DummyAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	DummyPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("DummyBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<DummyPort*>(src)->is_connected (dst_port);
}

bool
DummyAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<DummyPort*>(port)->is_physically_connected ();
}

int
DummyAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::vector<DummyPort*>& connected_ports = static_cast<DummyPort*>(port)->get_connections ();

	for (std::vector<DummyPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
DummyAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t** buf, void* port_buffer,
		uint32_t event_index)
{
	assert (buf && port_buffer);
	DummyMidiBuffer& source = * static_cast<DummyMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	DummyMidiEvent * const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size = event->size ();
	*buf = event->data ();
	return 0;
}

int
DummyAudioBackend::midi_event_put (
		void* port_buffer,
		pframes_t timestamp,
		const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	DummyMidiBuffer& dst = * static_cast<DummyMidiBuffer*>(port_buffer);
	if (dst.size () && (pframes_t)dst.back ()->timestamp () > timestamp) {
		fprintf (stderr, "DummyMidiBuffer: it's too late for this event.\n");
		return -1;
	}
	dst.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
DummyAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer && _running);
	return static_cast<DummyMidiBuffer*>(port_buffer)->size ();
}

void
DummyAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer && _running);
	DummyMidiBuffer * buf = static_cast<DummyMidiBuffer*>(port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
DummyAudioBackend::can_monitor_input () const
{
	return false;
}

int
DummyAudioBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
DummyAudioBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
DummyAudioBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
DummyAudioBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<DummyPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
DummyAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::get_latency_range (): invalid port.") << endmsg;
		LatencyRange r;
		r.min = 0;
		r.max = 0;
		return r;
	}
	return static_cast<DummyPort*>(port)->latency_range (for_playback);
}

/* Discovering physical ports */

bool
DummyAudioBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<DummyPort*>(port)->is_physical ();
}

void
DummyAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
DummyAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
DummyAudioBackend::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if (port->is_output () && port->is_physical ()) {
			switch (port->type ()) {
				case DataType::AUDIO: ++n_audio; break;
				case DataType::MIDI: ++n_midi; break;
				default: break;
			}
		}
	}
	ChanCount cc;
	cc.set (DataType::AUDIO, n_audio);
	cc.set (DataType::MIDI, n_midi);
	return cc;
}

ChanCount
DummyAudioBackend::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if (port->is_input () && port->is_physical ()) {
			switch (port->type ()) {
				case DataType::AUDIO: ++n_audio; break;
				case DataType::MIDI: ++n_midi; break;
				default: break;
			}
		}
	}
	ChanCount cc;
	cc.set (DataType::AUDIO, n_audio);
	cc.set (DataType::MIDI, n_midi);
	return cc;
}

/* Getting access to the data buffer for a port */

void*
DummyAudioBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	assert (port && _running);
	assert (valid_port (port));
	return static_cast<DummyPort*>(port)->get_buffer (nframes);
}

/* Engine Process */
void *
DummyAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_running = true;
	_processed_samples = 0;

	struct timeval clock1, clock2;
	::gettimeofday (&clock1, NULL);
	while (_running) {
		if (engine.process_callback (_samples_per_period)) {
			return 0;
		}
		_processed_samples += _samples_per_period;
		if (!_freewheeling) {
			::gettimeofday (&clock2, NULL);
			const int elapsed_time = (clock2.tv_sec - clock1.tv_sec) * 1000000 + (clock2.tv_usec - clock1.tv_usec);
			const int nomial_time = 1000000 * _samples_per_period / _samplerate;
			_dsp_load = elapsed_time / (float) nomial_time;
			if (elapsed_time < nomial_time) {
				Glib::usleep (nomial_time - elapsed_time);
			} else {
				Glib::usleep (100); // don't hog cpu
			}
		} else {
			_dsp_load = 1.0;
			Glib::usleep (100); // don't hog cpu
		}
		::gettimeofday (&clock1, NULL);
	}
	_running = false;
	return 0;
}


/******************************************************************************/

static boost::shared_ptr<DummyAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();

static ARDOUR::AudioBackendInfo _descriptor = {
	"Dummy",
	instantiate,
	deinstantiate,
	backend_factory,
	already_configured,
};

static boost::shared_ptr<AudioBackend>
backend_factory (AudioEngine& e)
{
	if (!_instance) {
		_instance.reset (new DummyAudioBackend (e, _descriptor));
	}
	return _instance;
}

static int
instantiate (const std::string& arg1, const std::string& /* arg2 */)
{
	s_instance_name = arg1;
	return 0;
}

static int
deinstantiate ()
{
	_instance.reset ();
	return 0;
}

static bool
already_configured ()
{
	return false;
}

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor ()
{
	return &_descriptor;
}


/******************************************************************************/
DummyPort::DummyPort (const std::string& name, PortFlags flags)
	: _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

DummyPort::~DummyPort () {
	disconnect_all ();
}


int DummyPort::connect (DummyPort *port)
{
	if (!port) {
		PBD::error << _("DummyPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("DummyPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("DummyPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("DummyPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("DummyPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("DummyPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}


void DummyPort::_connect (DummyPort *port, bool callback)
{
	_connections.push_back (port);
	if (callback) {
		port->_connect (this, false);
	}
}

int DummyPort::disconnect (DummyPort *port)
{
	if (!port) {
		PBD::error << _("DummyPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("DummyPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void DummyPort::_disconnect (DummyPort *port, bool callback)
{
	std::vector<DummyPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);

	assert (it != _connections.end ());

	_connections.erase (it);

	if (callback) {
		port->_disconnect (this, false);
	}
}


void DummyPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_connections.pop_back ();
	}
}

bool
DummyPort::is_connected (const DummyPort *port) const
{
	return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
}

bool DummyPort::is_physically_connected () const
{
	for (std::vector<DummyPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

/******************************************************************************/

DummyAudioPort::DummyAudioPort (const std::string& name, PortFlags flags)
	: DummyPort (name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
}

DummyAudioPort::~DummyAudioPort () { }

void* DummyAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<DummyPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			DummyAudioPort const * source = static_cast<const DummyAudioPort*>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<const DummyAudioPort*>(*it);
				assert (source && source->is_output ());
				Sample* dst = buffer ();
				const Sample* src = source->const_buffer ();
				for (uint32_t s = 0; s < n_samples; ++s, ++dst, ++src) {
					*dst += *src;
				}
			}
		}
	} else if (is_output () && is_physical () && is_terminal()) {
		memset (_buffer, 0, n_samples * sizeof (Sample));
	}
	return _buffer;
}


DummyMidiPort::DummyMidiPort (const std::string& name, PortFlags flags)
	: DummyPort (name, flags)
{
	_buffer.clear ();
}

DummyMidiPort::~DummyMidiPort () { }

void* DummyMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		_buffer.clear ();
		for (std::vector<DummyPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const DummyMidiBuffer src = static_cast<const DummyMidiPort*>(*i)->const_buffer ();
			for (DummyMidiBuffer::const_iterator it = src.begin (); it != src.end (); ++it) {
				_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
			}
		}
		std::sort (_buffer.begin (), _buffer.end ());
	} else if (is_output () && is_physical () && is_terminal()) {
		_buffer.clear ();
	}
	return &_buffer;
}

DummyMidiEvent::DummyMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
	, _data (0)
{
	if (size > 0) {
		_data = (uint8_t*) malloc (size);
	 memcpy (_data, data, size);
	}
}

DummyMidiEvent::DummyMidiEvent (const DummyMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
	, _data (0)
{
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

DummyMidiEvent::~DummyMidiEvent () {
	free (_data);
};
