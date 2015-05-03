/*
 * Copyright (C) 2015-2015 Robin Gareus <robin@gareus.org>
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

#include <regex.h>

#ifndef PLATFORM_WINDOWS
#include <sys/mman.h>
#include <sys/time.h>
#endif

#include <glibmm.h>

#include "portaudio_backend.h"
#include "rt_thread.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t PortAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> PortAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> PortAudioBackend::_audio_device_status;

PortAudioBackend::PortAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _pcmio (0)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _measure_latency (false)
	, _last_process_start (0)
	, _audio_device("")
	, _midi_driver_option(_("None"))
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _n_inputs (0)
	, _n_outputs (0)
	, _systemic_audio_input_latency (0)
	, _systemic_audio_output_latency (0)
	, _dsp_load (0)
	, _processed_samples (0)
	, _port_change_flag (false)
{
	_instance_name = s_instance_name;
	pthread_mutex_init (&_port_callback_mutex, 0);

	_pcmio = new PortAudioIO ();
}

PortAudioBackend::~PortAudioBackend ()
{
	delete _pcmio; _pcmio = 0;
	pthread_mutex_destroy (&_port_callback_mutex);
}

/* AUDIOBACKEND API */

std::string
PortAudioBackend::name () const
{
	return X_("PortAudio");
}

bool
PortAudioBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
PortAudioBackend::enumerate_devices () const
{
	_pcmio->discover();
	_audio_device_status.clear();
	std::map<int, std::string> devices;
	_pcmio->device_list(devices);

	for (std::map<int, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_audio_device == "") _audio_device = i->second;
		_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _audio_device_status;
}

std::vector<float>
PortAudioBackend::available_sample_rates (const std::string&) const
{
	std::vector<float> sr;
	_pcmio->available_sample_rates(name_to_id(_audio_device), sr);
	return sr;
}

std::vector<uint32_t>
PortAudioBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
	_pcmio->available_buffer_sizes(name_to_id(_audio_device), bs);
	return bs;
}

uint32_t
PortAudioBackend::available_input_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

uint32_t
PortAudioBackend::available_output_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

bool
PortAudioBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
PortAudioBackend::can_change_buffer_size_when_running () const
{
	return false; // TODO
}

int
PortAudioBackend::set_device_name (const std::string& d)
{
	_audio_device = d;
	return 0;
}

int
PortAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) { return -1; }
	// TODO check if it's in the list of valid SR
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
PortAudioBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (bs);
	return 0;
}

int
PortAudioBackend::set_interleaved (bool yn)
{
	if (!yn) { return 0; }
	return -1;
}

int
PortAudioBackend::set_input_channels (uint32_t cc)
{
	_n_inputs = cc;
	return 0;
}

int
PortAudioBackend::set_output_channels (uint32_t cc)
{
	_n_outputs = cc;
	return 0;
}

int
PortAudioBackend::set_systemic_input_latency (uint32_t sl)
{
	_systemic_audio_input_latency = sl;
	return 0;
}

int
PortAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	_systemic_audio_output_latency = sl;
	return 0;
}

/* Retrieving parameters */
std::string
PortAudioBackend::device_name () const
{
	return _audio_device;
}

float
PortAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
PortAudioBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
PortAudioBackend::interleaved () const
{
	return false;
}

uint32_t
PortAudioBackend::input_channels () const
{
	return _n_inputs;
}

uint32_t
PortAudioBackend::output_channels () const
{
	return _n_outputs;
}

uint32_t
PortAudioBackend::systemic_input_latency () const
{
	return _systemic_audio_input_latency;
}

uint32_t
PortAudioBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

/* MIDI */

std::vector<std::string>
PortAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		//_midi_options.push_back (_("PortMidi"));
		_midi_options.push_back (_("None"));
	}
	return _midi_options;
}

int
PortAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != _("None") /* && opt != _("PortMidi")*/) {
		return -1;
	}
	_midi_driver_option = opt;
	return 0;
}

std::string
PortAudioBackend::midi_option () const
{
	return _midi_driver_option;
}

/* State Control */

static void * pthread_process (void *arg)
{
	PortAudioBackend *d = static_cast<PortAudioBackend *>(arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
PortAudioBackend::_start (bool for_latency_measurement)
{
	if (!_active && _run) {
		// recover from 'halted', reap threads
		stop();
	}

	if (_active || _run) {
		PBD::error << _("PortAudioBackend: already active.") << endmsg;
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("PortAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_system_inputs.clear();
		_system_outputs.clear();
		_system_midi_in.clear();
		_system_midi_out.clear();
		_ports.clear();
	}

	/* reset internal state */
	_dsp_load = 0;
	_freewheeling = false;
	_freewheel = false;
	_last_process_start = 0;

	_pcmio->pcm_setup (name_to_id(_audio_device), name_to_id(_audio_device), _samplerate, _samples_per_period);

	switch (_pcmio->state ()) {
		case 0: /* OK */ break;
		case -1: PBD::error << _("PortAudioBackend: failed to open device.") << endmsg; break;
		default: PBD::error << _("PortAudioBackend: initialization failed.") << endmsg; break;
	}
	if (_pcmio->state ()) {
		return -1;
	}

	if (_n_outputs != _pcmio->n_playback_channels ()) {
		_n_outputs = _pcmio->n_playback_channels ();
		PBD::info << _("PortAudioBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmio->n_capture_channels ()) {
		_n_inputs = _pcmio->n_capture_channels ();
		PBD::info << _("PortAudioBackend: adjusted input channel count to match device.") << endmsg;
	}
#if 0
	if (_pcmio->samples_per_period() != _samples_per_period) {
		_samples_per_period = _pcmio->samples_per_period();
		PBD::warning << _("PortAudioBackend: samples per period does not match.") << endmsg;
	}
#endif

	if (_pcmio->sample_rate() != _samplerate) {
		_samplerate = _pcmio->sample_rate();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("PortAudioBackend: sample rate does not match.") << endmsg;
	}

	_measure_latency = for_latency_measurement;

	_run = true;
	_port_change_flag = false;

	// TODO MIDI

	if (register_system_audio_ports()) {
		PBD::error << _("PortAudioBackend: failed to register system ports.") << endmsg;
		_run = false;
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("PortAudioBackend: Could not re-establish ports.") << endmsg;
		_run = false;
		return -1;
	}

	engine.reconnect_ports ();
	_run = true;
	_port_change_flag = false;

	if (_realtime_pthread_create (SCHED_FIFO, -20, 100000,
				&_main_thread, pthread_process, this))
	{
		if (pthread_create (&_main_thread, NULL, pthread_process, this))
		{
			PBD::error << _("PortAudioBackend: failed to create process thread.") << endmsg;
			_run = false;
			return -1;
		} else {
			PBD::warning << _("PortAudioBackend: cannot acquire realtime permissions.") << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_active) {
		PBD::error << _("PortAudioBackend: failed to start.") << endmsg;
		_pcmio->pcm_stop();
		_run = false;
		unregister_ports();
		_active = false;
		return -1;
	}

	return 0;
}

int
PortAudioBackend::stop ()
{
	void *status;
	if (!_run) {
		return 0;
	}

	_run = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("PortAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}


	unregister_ports();

	return (_active == false) ? 0 : -1;
}

int
PortAudioBackend::freewheel (bool onoff)
{
	if (onoff == _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;
	return 0;
}

float
PortAudioBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
PortAudioBackend::raw_buffer_size (DataType t)
{
	switch (t) {
		case DataType::AUDIO:
			return _samples_per_period * sizeof(Sample);
		case DataType::MIDI:
			return _max_buffer_size; // XXX not really limited
	}
	return 0;
}

/* Process time */
framepos_t
PortAudioBackend::sample_time ()
{
	return _processed_samples;
}

framepos_t
PortAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
PortAudioBackend::samples_since_cycle_start ()
{
	if (!_active || !_run || _freewheeling || _freewheel) {
		return 0;
	}
	if (_last_process_start == 0) {
		return 0;
	}

	const int64_t elapsed_time_us = g_get_monotonic_time() - _last_process_start;
	return std::max((pframes_t)0, (pframes_t)rint(1e-6 * elapsed_time_us * _samplerate));
}

int
PortAudioBackend::name_to_id(std::string device_name) const {
	uint32_t device_id = UINT32_MAX;
	std::map<int, std::string> devices;
	_pcmio->device_list(devices);

	for (std::map<int, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (i->second == device_name) {
			device_id = i->first;
			break;
		}
	}
	return device_id;
}

void *
PortAudioBackend::portaudio_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
PortAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t thread_id;
	pthread_attr_t attr;
	size_t stacksize = 100000;

	ThreadData* td = new ThreadData (this, func, stacksize);

	if (_realtime_pthread_create (SCHED_FIFO, -21, stacksize,
				&thread_id, portaudio_process_thread, td)) {
		pthread_attr_init (&attr);
		pthread_attr_setstacksize (&attr, stacksize);
		if (pthread_create (&thread_id, &attr, portaudio_process_thread, td)) {
			PBD::error << _("AudioEngine: cannot create process thread.") << endmsg;
			pthread_attr_destroy (&attr);
			return -1;
		}
		pthread_attr_destroy (&attr);
	}

	_threads.push_back (thread_id);
	return 0;
}

int
PortAudioBackend::join_process_threads ()
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
PortAudioBackend::in_process_thread ()
{
	if (pthread_equal (_main_thread, pthread_self()) != 0) {
		return true;
	}

	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i)
	{
		if (pthread_equal (*i, pthread_self ()) != 0) {
			return true;
		}
	}
	return false;
}

uint32_t
PortAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
PortAudioBackend::update_latencies ()
{
	// trigger latency callback in RT thread (locked graph)
	port_connect_add_remove_callback();
}

/* PORTENGINE API */

void*
PortAudioBackend::private_handle () const
{
	return NULL;
}

const std::string&
PortAudioBackend::my_name () const
{
	return _instance_name;
}

bool
PortAudioBackend::available () const
{
	return _run && _active;
}

uint32_t
PortAudioBackend::port_name_size () const
{
	return 256;
}

int
PortAudioBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<PamPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
PortAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<PamPort*>(port)->name ();
}

PortEngine::PortHandle
PortAudioBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle) find_port (name);
	return port;
}

int
PortAudioBackend::get_ports (
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
		PamPort* port = _ports[i];
		if ((port->type () == type) && flags == (port->flags () & flags)) {
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
PortAudioBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<PamPort*>(port)->type ();
}

PortEngine::PortHandle
PortAudioBackend::register_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
PortAudioBackend::add_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("PortAudioBackend::register_port: Port already exists:")
				<< " (" << name << ")" << endmsg;
		return 0;
	}
	PamPort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new PortAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new PortMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << _("PortAudioBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.push_back (port);

	return port;
}

void
PortAudioBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!_run) {
		return;
	}
	PamPort* port = static_cast<PamPort*>(port_handle);
	std::vector<PamPort*>::iterator i = std::find (_ports.begin (), _ports.end (), static_cast<PamPort*>(port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("PortAudioBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all(port_handle);
	_ports.erase (i);
	delete port;
}

int
PortAudioBackend::register_system_audio_ports()
{
	LatencyRange lr;

	const uint32_t a_ins = _n_inputs;
	const uint32_t a_out = _n_outputs;

	// XXX PA reported stream latencies don't match measurements
	const uint32_t portaudio_reported_input_latency =  _samples_per_period ; //  _pcmio->capture_latency();
	const uint32_t portaudio_reported_output_latency = /* _samples_per_period + */ _pcmio->playback_latency();

	/* audio ports */
	lr.min = lr.max = portaudio_reported_input_latency + (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (uint32_t i = 0; i < a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		_system_inputs.push_back(static_cast<PortAudioPort*>(p));
	}

	lr.min = lr.max = portaudio_reported_output_latency + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (uint32_t i = 0; i < a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_outputs.push_back(static_cast<PamPort*>(p));
	}
	return 0;
}

void
PortAudioBackend::unregister_ports (bool system_only)
{
	size_t i = 0;
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();
	while (i <  _ports.size ()) {
		PamPort* port = _ports[i];
		if (! system_only || (port->is_physical () && port->is_terminal ())) {
			port->disconnect_all ();
			delete port;
			_ports.erase (_ports.begin() + i);
		} else {
			++i;
		}
	}
}

int
PortAudioBackend::connect (const std::string& src, const std::string& dst)
{
	PamPort* src_port = find_port (src);
	PamPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("PortAudioBackend::connect: Invalid Source port:")
				<< " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("PortAudioBackend::connect: Invalid Destination port:")
			<< " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
PortAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	PamPort* src_port = find_port (src);
	PamPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("PortAudioBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
PortAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("PortAudioBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("PortAudioBackend::connect: Invalid Destination Port")
			<< " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<PamPort*>(src)->connect (dst_port);
}

int
PortAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("PortAudioBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<PamPort*>(src)->disconnect (dst_port);
}

int
PortAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<PamPort*>(port)->disconnect_all ();
	return 0;
}

bool
PortAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<PamPort*>(port)->is_connected ();
}

bool
PortAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("PortAudioBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<PamPort*>(src)->is_connected (dst_port);
}

bool
PortAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<PamPort*>(port)->is_physically_connected ();
}

int
PortAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PortAudioBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::vector<PamPort*>& connected_ports = static_cast<PamPort*>(port)->get_connections ();

	for (std::vector<PamPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
PortAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t** buf, void* port_buffer,
		uint32_t event_index)
{
	if (!buf || !port_buffer) return -1;
	PortMidiBuffer& source = * static_cast<PortMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	PortMidiEvent * const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size = event->size ();
	*buf = event->data ();
	return 0;
}

int
PortAudioBackend::midi_event_put (
		void* port_buffer,
		pframes_t timestamp,
		const uint8_t* buffer, size_t size)
{
	if (!buffer || !port_buffer) return -1;
	PortMidiBuffer& dst = * static_cast<PortMidiBuffer*>(port_buffer);
	if (dst.size () && (pframes_t)dst.back ()->timestamp () > timestamp) {
#ifndef NDEBUG
		// nevermind, ::get_buffer() sorts events
		fprintf (stderr, "PortMidiBuffer: unordered event: %d > %d\n",
				(pframes_t)dst.back ()->timestamp (), timestamp);
#endif
	}
	dst.push_back (boost::shared_ptr<PortMidiEvent>(new PortMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
PortAudioBackend::get_midi_event_count (void* port_buffer)
{
	if (!port_buffer) return 0;
	return static_cast<PortMidiBuffer*>(port_buffer)->size ();
}

void
PortAudioBackend::midi_clear (void* port_buffer)
{
	if (!port_buffer) return;
	PortMidiBuffer * buf = static_cast<PortMidiBuffer*>(port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
PortAudioBackend::can_monitor_input () const
{
	return false;
}

int
PortAudioBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
PortAudioBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
PortAudioBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
PortAudioBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("PamPort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<PamPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
PortAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::error << _("PamPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}
	PamPort* p = static_cast<PamPort*>(port);
	assert(p);

	r = p->latency_range (for_playback);
	// TODO MIDI
	if (p->is_physical() && p->is_terminal() && p->type() == DataType::AUDIO) {
		if (p->is_input() && for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
		if (p->is_output() && !for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
	}
	return r;
}

/* Discovering physical ports */

bool
PortAudioBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("PamPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<PamPort*>(port)->is_physical ();
}

void
PortAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		PamPort* port = _ports[i];
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
PortAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		PamPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
PortAudioBackend::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		PamPort* port = _ports[i];
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
PortAudioBackend::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		PamPort* port = _ports[i];
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
PortAudioBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	if (!port || !valid_port (port)) return NULL;
	return static_cast<PamPort*>(port)->get_buffer (nframes);
}


void *
PortAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active = true;
	_processed_samples = 0;

	uint64_t clock1, clock2;
	const int64_t nomial_time = 1e6 * _samples_per_period / _samplerate;

	manager.registration_callback();
	manager.graph_order_callback();

	if (_pcmio->pcm_start()) {
		_pcmio->pcm_stop ();
		_active = false;
		engine.halted_callback("PortAudio I/O error.");
	}

	while (_run) {

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
		}

		if (!_freewheel) {

			switch (_pcmio->next_cycle (_samples_per_period)) {
				case 0: // OK
					break;
				case 1:
#ifndef NDEBUG
					printf("PortAudio: Xrun\n");
#endif
					engine.Xrun ();
					break;
				default:
					PBD::error << _("PortAudioBackend: I/O error. Audio Process Terminated.") << endmsg;
					break;
			}

			uint32_t i = 0;
			clock1 = g_get_monotonic_time();

			/* get audio */
			i = 0;
			for (std::vector<PamPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++i) {
				_pcmio->get_capture_channel (i, (float*)((*it)->get_buffer(_samples_per_period)), _samples_per_period);
			}

			/* de-queue incoming midi*/
			i=0;
			for (std::vector<PamPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
				PortMidiBuffer* mbuf = static_cast<PortMidiBuffer*>((*it)->get_buffer(0));
				mbuf->clear();
			}

			/* clear output buffers */
			for (std::vector<PamPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
				memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
			}

			/* call engine process callback */
			_last_process_start = g_get_monotonic_time();
			if (engine.process_callback (_samples_per_period)) {
				_pcmio->pcm_stop ();
				_active = false;
				return 0;
			}
#if 0
			/* mixdown midi */
			for (std::vector<PamPort*>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
				static_cast<PortBackendMidiPort*>(*it)->next_period();
			}

			/* queue outgoing midi */
			i = 0;
			for (std::vector<PamPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
				// TODO
			}
#endif

			/* write back audio */
			i = 0;
			for (std::vector<PamPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
				_pcmio->set_playback_channel (i, (float const*)(*it)->get_buffer (_samples_per_period), _samples_per_period);
			}

			_processed_samples += _samples_per_period;

			/* calculate DSP load */
			clock2 = g_get_monotonic_time();
			const int64_t elapsed_time = clock2 - clock1;
			_dsp_load = elapsed_time / (float) nomial_time;

		} else {
			// Freewheelin'

			// zero audio input buffers
			for (std::vector<PamPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
				memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
			}

			clock1 = g_get_monotonic_time();

			// TODO clear midi or stop midi recv when entering fwheelin'

			_last_process_start = 0;
			if (engine.process_callback (_samples_per_period)) {
				_pcmio->pcm_stop ();
				_active = false;
				return 0;
			}

			// drop all outgoing MIDI messages
			for (std::vector<PamPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
					void *bptr = (*it)->get_buffer(0);
					midi_clear(bptr);
			}

			_dsp_load = 1.0;
			Glib::usleep (100); // don't hog cpu
		}

		bool connections_changed = false;
		bool ports_changed = false;
		if (!pthread_mutex_trylock (&_port_callback_mutex)) {
			if (_port_change_flag) {
				ports_changed = true;
				_port_change_flag = false;
			}
			if (!_port_connection_queue.empty ()) {
				connections_changed = true;
			}
			while (!_port_connection_queue.empty ()) {
				PortConnectData *c = _port_connection_queue.back ();
				manager.connect_callback (c->a, c->b, c->c);
				_port_connection_queue.pop_back ();
				delete c;
			}
			pthread_mutex_unlock (&_port_callback_mutex);
		}
		if (ports_changed) {
			manager.registration_callback();
		}
		if (connections_changed) {
			manager.graph_order_callback();
		}
		if (connections_changed || ports_changed) {
			engine.latency_callback(false);
			engine.latency_callback(true);
		}

	}
	_pcmio->pcm_stop ();
	_active = false;
	if (_run) {
		engine.halted_callback("PortAudio I/O error.");
	}
	return 0;
}


/******************************************************************************/

static boost::shared_ptr<PortAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	"PortAudio",
	instantiate,
	deinstantiate,
	backend_factory,
	already_configured,
	available
};

static boost::shared_ptr<AudioBackend>
backend_factory (AudioEngine& e)
{
	if (!_instance) {
		_instance.reset (new PortAudioBackend (e, _descriptor));
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

static bool
available ()
{
	return true;
}

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor ()
{
	return &_descriptor;
}


/******************************************************************************/
PamPort::PamPort (PortAudioBackend &b, const std::string& name, PortFlags flags)
	: _osx_backend (b)
	, _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

PamPort::~PamPort () {
	disconnect_all ();
}


int PamPort::connect (PamPort *port)
{
	if (!port) {
		PBD::error << _("PamPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("PamPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("PamPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("PamPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("PamPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("PamPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}


void PamPort::_connect (PamPort *port, bool callback)
{
	_connections.push_back (port);
	if (callback) {
		port->_connect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), true);
	}
}

int PamPort::disconnect (PamPort *port)
{
	if (!port) {
		PBD::error << _("PamPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("PamPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void PamPort::_disconnect (PamPort *port, bool callback)
{
	std::vector<PamPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);

	assert (it != _connections.end ());

	_connections.erase (it);

	if (callback) {
		port->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void PamPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  _connections.back ()->name(), false);
		_connections.pop_back ();
	}
}

bool
PamPort::is_connected (const PamPort *port) const
{
	return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
}

bool PamPort::is_physically_connected () const
{
	for (std::vector<PamPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

/******************************************************************************/

PortAudioPort::PortAudioPort (PortAudioBackend &b, const std::string& name, PortFlags flags)
	: PamPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
#ifndef PLATFORM_WINDOWS
	mlock(_buffer, sizeof (_buffer));
#endif
}

PortAudioPort::~PortAudioPort () { }

void* PortAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<PamPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			PortAudioPort const * source = static_cast<const PortAudioPort*>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<const PortAudioPort*>(*it);
				assert (source && source->is_output ());
				Sample* dst = buffer ();
				const Sample* src = source->const_buffer ();
				for (uint32_t s = 0; s < n_samples; ++s, ++dst, ++src) {
					*dst += *src;
				}
			}
		}
	}
	return _buffer;
}


PortMidiPort::PortMidiPort (PortAudioBackend &b, const std::string& name, PortFlags flags)
	: PamPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();
}

PortMidiPort::~PortMidiPort () { }

struct MidiEventSorter {
	bool operator() (const boost::shared_ptr<PortMidiEvent>& a, const boost::shared_ptr<PortMidiEvent>& b) {
		return *a < *b;
	}
};

void* PortMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		for (std::vector<PamPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const PortMidiBuffer * src = static_cast<const PortMidiPort*>(*i)->const_buffer ();
			for (PortMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (boost::shared_ptr<PortMidiEvent>(new PortMidiEvent (**it)));
			}
		}
		std::sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}
	return &(_buffer[_bufperiod]);
}

PortMidiEvent::PortMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
	, _data (0)
{
	if (size > 0) {
		_data = (uint8_t*) malloc (size);
		memcpy (_data, data, size);
	}
}

PortMidiEvent::PortMidiEvent (const PortMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
	, _data (0)
{
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

PortMidiEvent::~PortMidiEvent () {
	free (_data);
};
