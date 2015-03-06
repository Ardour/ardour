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

#include <regex.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <glibmm.h>

#include "coreaudio_backend.h"
#include "rt_thread.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t CoreAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> CoreAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> CoreAudioBackend::_audio_device_status;
std::vector<AudioBackend::DeviceStatus> CoreAudioBackend::_midi_device_status;


static void hw_changed_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->hw_changed_callback();
}

CoreAudioBackend::CoreAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _run (false)
	, _active_ca (false)
	, _active_fw (false)
	, _freewheeling (false)
	, _freewheel (false)
	, _freewheel_ack (false)
	, _reinit_thread_callback (false)
	, _measure_latency (false)
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
	pthread_mutex_init (&_process_callback_mutex, 0);

	_pcmio = new CoreAudioPCM ();
	_midiio = new CoreMidiIo ();

	_pcmio->set_hw_changed_callback (hw_changed_callback_ptr, this);
	_pcmio->discover();
	_midiio->discover();
}

CoreAudioBackend::~CoreAudioBackend ()
{
	delete _pcmio; _pcmio = 0;
	delete _midiio; _midiio = 0;
	pthread_mutex_destroy (&_port_callback_mutex);
	pthread_mutex_destroy (&_process_callback_mutex);
}

/* AUDIOBACKEND API */

std::string
CoreAudioBackend::name () const
{
	return X_("CoreAudio");
}

bool
CoreAudioBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
CoreAudioBackend::enumerate_devices () const
{
	_audio_device_status.clear();
	std::map<size_t, std::string> devices;
	_pcmio->device_list(devices);

	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_audio_device == "") _audio_device = i->second;
		_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _audio_device_status;
}

std::vector<float>
CoreAudioBackend::available_sample_rates (const std::string&) const
{
	std::vector<float> sr;
	_pcmio->available_sample_rates(name_to_id(_audio_device), sr);
	return sr;
}

std::vector<uint32_t>
CoreAudioBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
	_pcmio->available_buffer_sizes(name_to_id(_audio_device), bs);
	return bs;
}

uint32_t
CoreAudioBackend::available_input_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

uint32_t
CoreAudioBackend::available_output_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

bool
CoreAudioBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
CoreAudioBackend::can_change_buffer_size_when_running () const
{
	return false;
}

int
CoreAudioBackend::set_device_name (const std::string& d)
{
	_audio_device = d;
	const float sr = _pcmio->current_sample_rate(name_to_id(_audio_device));
	if (sr > 0) { set_sample_rate(sr); }
	return 0;
}

int
CoreAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) { return -1; }
	// TODO check if it's in the list of valid SR
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
CoreAudioBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (bs);
	return 0;
}

int
CoreAudioBackend::set_interleaved (bool yn)
{
	if (!yn) { return 0; }
	return -1;
}

int
CoreAudioBackend::set_input_channels (uint32_t cc)
{
	_n_inputs = cc;
	return 0;
}

int
CoreAudioBackend::set_output_channels (uint32_t cc)
{
	_n_outputs = cc;
	return 0;
}

int
CoreAudioBackend::set_systemic_input_latency (uint32_t sl)
{
	_systemic_audio_input_latency = sl;
	return 0;
}

int
CoreAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	_systemic_audio_output_latency = sl;
	return 0;
}

int
CoreAudioBackend::set_systemic_midi_input_latency (std::string const device, uint32_t sl)
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->systemic_input_latency = sl;
	return 0;
}

int
CoreAudioBackend::set_systemic_midi_output_latency (std::string const device, uint32_t sl)
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->systemic_output_latency = sl;
	return 0;
}

/* Retrieving parameters */
std::string
CoreAudioBackend::device_name () const
{
	return _audio_device;
}

float
CoreAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
CoreAudioBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
CoreAudioBackend::interleaved () const
{
	return false;
}

uint32_t
CoreAudioBackend::input_channels () const
{
	return _n_inputs;
}

uint32_t
CoreAudioBackend::output_channels () const
{
	return _n_outputs;
}

uint32_t
CoreAudioBackend::systemic_input_latency () const
{
	return _systemic_audio_input_latency;
}

uint32_t
CoreAudioBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

uint32_t
CoreAudioBackend::systemic_midi_input_latency (std::string const device) const
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return 0;
	return nfo->systemic_input_latency;
}

uint32_t
CoreAudioBackend::systemic_midi_output_latency (std::string const device) const
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return 0;
	return nfo->systemic_output_latency;
}

/* MIDI */
struct CoreAudioBackend::CoreMidiDeviceInfo *
CoreAudioBackend::midi_device_info(std::string const name) const {
	return 0;
}

std::vector<std::string>
CoreAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		_midi_options.push_back (_("CoreMidi"));
		_midi_options.push_back (_("None"));
	}
	return _midi_options;
}

std::vector<AudioBackend::DeviceStatus>
CoreAudioBackend::enumerate_midi_devices () const
{
	_midi_device_status.clear();
	std::map<std::string, std::string> devices;
	//_midi_device_status.push_back (DeviceStatus (_("CoreMidi"), true));
	return _midi_device_status;
}

int
CoreAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != _("None") && opt != _("CoreMidi")) {
		return -1;
	}
	_midi_driver_option = opt;
	return 0;
}

std::string
CoreAudioBackend::midi_option () const
{
	return _midi_driver_option;
}

int
CoreAudioBackend::set_midi_device_enabled (std::string const device, bool enable)
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->enabled = enable;
	return 0;
}

bool
CoreAudioBackend::midi_device_enabled (std::string const device) const
{
	struct CoreMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return false;
	return nfo->enabled;
}

void
CoreAudioBackend::launch_control_app ()
{
    _pcmio->launch_control_app(name_to_id(_audio_device));
}

/* State Control */

static void * pthread_freewheel (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend *>(arg);
	d->freewheel_thread ();
	pthread_exit (0);
	return 0;
}

static int process_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	return d->process_callback();
}

static void error_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->error_callback();
}

static void midi_port_change (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend *>(arg);
	d->coremidi_rediscover ();
}

int
CoreAudioBackend::_start (bool for_latency_measurement)
{
	if ((!_active_ca || !_active_fw)  && _run) {
		// recover from 'halted', reap threads
		stop();
	}

	if (_active_ca || _active_fw || _run) {
		PBD::error << _("CoreAudioBackend: already active.") << endmsg;
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("CoreAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_system_inputs.clear();
		_system_outputs.clear();
		_system_midi_in.clear();
		_system_midi_out.clear();
		_ports.clear();
	}

	uint32_t device_id = name_to_id(_audio_device);

	assert(_active_ca == false);
	assert(_active_fw == false);

	_freewheel_ack = false;
	_reinit_thread_callback = true;

	_pcmio->set_error_callback (error_callback_ptr, this);
	_pcmio->pcm_start (device_id, device_id, _samplerate, _samples_per_period, process_callback_ptr, this);

	switch (_pcmio->state ()) {
		case 0: /* OK */ break;
		case -1: PBD::error << _("CoreAudioBackend: failed to open device.") << endmsg; break;
		default: PBD::error << _("CoreAudioBackend: initialization failed.") << endmsg; break;
	}
	if (_pcmio->state ()) {
		return -1;
	}

	if (_n_outputs != _pcmio->n_playback_channels ()) {
		if (_n_outputs == 0) {
		 _n_outputs = _pcmio->n_playback_channels ();
		} else {
		 _n_outputs = std::min (_n_outputs, _pcmio->n_playback_channels ());
		}
		PBD::warning << _("CoreAudioBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmio->n_capture_channels ()) {
		if (_n_inputs == 0) {
		 _n_inputs = _pcmio->n_capture_channels ();
		} else {
		 _n_inputs = std::min (_n_inputs, _pcmio->n_capture_channels ());
		}
		PBD::warning << _("CoreAudioBackend: adjusted input channel count to match device.") << endmsg;
	}

#if 0 // TODO
	if (_pcmio->sample_per_period() != _samples_per_period) {
		_samples_per_period = _pcmio->sample_per_period();
		PBD::warning << _("CoreAudioBackend: samples per period does not match.") << endmsg;
	}

	if (_pcmio->samplerate() != _samplerate) {
		_samplerate = _pcmio->samplerate();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("CoreAudioBackend: sample rate does not match.") << endmsg;
	}
#endif

	_measure_latency = for_latency_measurement;

	_preinit = true;
	_run = true;
	_port_change_flag = false;

	if (_midi_driver_option == _("CoreMidi")) {
		_midiio->set_port_changed_callback(midi_port_change, this);
		_midiio->discover();
	}

	if (register_system_audio_ports()) {
		PBD::error << _("CoreAudioBackend: failed to register system ports.") << endmsg;
		_run = false;
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("CoreAudioBackend: Could not re-establish ports.") << endmsg;
		_run = false;
		return -1;
	}

	engine.reconnect_ports ();

	if (pthread_create (&_freeewheel_thread, NULL, pthread_freewheel, this))
	{
		PBD::error << _("CoreAudioBackend: failed to create process thread.") << endmsg;
		delete _pcmio; _pcmio = 0;
		_run = false;
		return -1;
	}

	int timeout = 5000;
	while ((!_active_ca || !_active_fw) && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0) {
		PBD::error << _("CoreAudioBackend: failed to start.") << endmsg;
	}

	if (!_active_fw) {
		PBD::error << _("CoreAudioBackend: failed to start freewheeling thread.") << endmsg;
		_run = false;
		_pcmio->pcm_stop();
		unregister_ports();
		_active_ca = false;
		_active_fw = false;
		return -1;
	}

	if (!_active_ca) {
		PBD::error << _("CoreAudioBackend: failed to start coreaudio.") << endmsg;
		stop();
		_run = false;
		return -1;
	}
	_preinit = false;

	return 0;
}

int
CoreAudioBackend::stop ()
{
	void *status;
	if (!_run) {
		return 0;
	}

	_run = false;
	_pcmio->pcm_stop();
	_midiio->set_port_changed_callback(NULL, NULL);

	if (pthread_join (_freeewheel_thread, &status)) {
		PBD::error << _("CoreAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}

	unregister_ports();

	_active_ca = false;
	_active_fw = false; // ??

	return 0;
}

int
CoreAudioBackend::freewheel (bool onoff)
{
	if (onoff == _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;
	return 0;
}

float
CoreAudioBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
CoreAudioBackend::raw_buffer_size (DataType t)
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
CoreAudioBackend::sample_time ()
{
	return _processed_samples;
}

framepos_t
CoreAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
CoreAudioBackend::samples_since_cycle_start ()
{
	return 0;
}

uint32_t
CoreAudioBackend::name_to_id(std::string device_name) const {
	uint32_t device_id = UINT32_MAX;
	std::map<size_t, std::string> devices;
	_pcmio->device_list(devices);

	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (i->second == device_name) {
			device_id = i->first;
			break;
		}
	}
	return device_id;
}

void *
CoreAudioBackend::coreaudio_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
CoreAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t thread_id;
	pthread_attr_t attr;
	size_t stacksize = 100000;

	ThreadData* td = new ThreadData (this, func, stacksize);

	if (_realtime_pthread_create (SCHED_FIFO, -21, stacksize,
				&thread_id, coreaudio_process_thread, td)) {
		pthread_attr_init (&attr);
		pthread_attr_setstacksize (&attr, stacksize);
		if (pthread_create (&thread_id, &attr, coreaudio_process_thread, td)) {
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
CoreAudioBackend::join_process_threads ()
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
CoreAudioBackend::in_process_thread ()
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
CoreAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
CoreAudioBackend::update_latencies ()
{
	// trigger latency callback in RT thread (locked graph)
	port_connect_add_remove_callback();
}

/* PORTENGINE API */

void*
CoreAudioBackend::private_handle () const
{
	return NULL;
}

const std::string&
CoreAudioBackend::my_name () const
{
	return _instance_name;
}

bool
CoreAudioBackend::available () const
{
	return _run && _active_fw && _active_ca;
}

uint32_t
CoreAudioBackend::port_name_size () const
{
	return 256;
}

int
CoreAudioBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<CoreBackendPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
CoreAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<CoreBackendPort*>(port)->name ();
}

PortEngine::PortHandle
CoreAudioBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle) find_port (name);
	return port;
}

int
CoreAudioBackend::get_ports (
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
		CoreBackendPort* port = _ports[i];
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
CoreAudioBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<CoreBackendPort*>(port)->type ();
}

PortEngine::PortHandle
CoreAudioBackend::register_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
CoreAudioBackend::add_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("CoreAudioBackend::register_port: Port already exists:")
				<< " (" << name << ")" << endmsg;
		return 0;
	}
	CoreBackendPort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new CoreAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new CoreMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << _("CoreAudioBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.push_back (port);

	return port;
}

void
CoreAudioBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!_run) {
		return;
	}
	CoreBackendPort* port = static_cast<CoreBackendPort*>(port_handle);
	std::vector<CoreBackendPort*>::iterator i = std::find (_ports.begin (), _ports.end (), static_cast<CoreBackendPort*>(port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("CoreAudioBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all(port_handle);
	_ports.erase (i);
	delete port;
}

int
CoreAudioBackend::register_system_audio_ports()
{
	LatencyRange lr;
	printf("COREAUDIO LATENCY: i:%d, o:%d\n",
			_pcmio->get_latency(name_to_id(_audio_device), true),
			_pcmio->get_latency(name_to_id(_audio_device), false));

	//TODO set latencies
	//TODO query port names

	const int a_ins = _n_inputs > 0 ? _n_inputs : 2;
	const int a_out = _n_outputs > 0 ? _n_outputs : 2;

	/* audio ports */
	lr.min = lr.max = _samples_per_period + (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (int i = 1; i <= a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		_system_inputs.push_back(static_cast<CoreBackendPort*>(p));
	}

	lr.min = lr.max = _samples_per_period + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_outputs.push_back(static_cast<CoreBackendPort*>(p));
	}
	return 0;
}

int
CoreAudioBackend::register_system_midi_ports()
{
	int midi_ins = _system_midi_out.size();
	int midi_outs =  _system_midi_in.size();

	//TODO query port names
	for (uint32_t i = midi_ins; i < _midiio->n_midi_outputs(); ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_playback_%d", ++midi_ins);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) {
			continue;
		}
		LatencyRange lr;
		lr.min = lr.max = _samples_per_period; // TODO add per-port midi-systemic latency
		set_latency_range (p, false, lr);
		_system_midi_out.push_back(static_cast<CoreBackendPort*>(p));
	}

	for (uint32_t i = midi_outs; i < _midiio->n_midi_inputs(); ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_capture_%d", ++midi_outs);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) {
			continue;
		}
		LatencyRange lr;
		lr.min = lr.max = _samples_per_period; // TODO add per-port midi-systemic latency
		set_latency_range (p, false, lr);
		_system_midi_in.push_back(static_cast<CoreBackendPort*>(p));
	}

	return 0;
}

void
CoreAudioBackend::coremidi_rediscover()
{
	if (!_run) { return; }
	assert(_midi_driver_option == _("CoreMidi"));

	pthread_mutex_lock (&_process_callback_mutex);

	// TODO maintain device-specific connections, rather
	// than re-map.
	while (_system_midi_out.size() > _midiio->n_midi_outputs()) {
		CoreBackendPort* p = _system_midi_out.back();
		_system_midi_out.pop_back();
		unregister_port(p);
	}

	while (_system_midi_in.size() > _midiio->n_midi_inputs()) {
		CoreBackendPort* p = _system_midi_in.back();
		_system_midi_in.pop_back();
		unregister_port(p);
	}

	register_system_midi_ports();

	_port_change_flag = true;
	pthread_mutex_unlock (&_process_callback_mutex);
}

void
CoreAudioBackend::unregister_ports (bool system_only)
{
	size_t i = 0;
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();
	while (i <  _ports.size ()) {
		CoreBackendPort* port = _ports[i];
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
CoreAudioBackend::connect (const std::string& src, const std::string& dst)
{
	CoreBackendPort* src_port = find_port (src);
	CoreBackendPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("CoreAudioBackend::connect: Invalid Source port:")
				<< " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("CoreAudioBackend::connect: Invalid Destination port:")
			<< " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
CoreAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	CoreBackendPort* src_port = find_port (src);
	CoreBackendPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("CoreAudioBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
CoreAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	CoreBackendPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("CoreAudioBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("CoreAudioBackend::connect: Invalid Destination Port")
			<< " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<CoreBackendPort*>(src)->connect (dst_port);
}

int
CoreAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	CoreBackendPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("CoreAudioBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<CoreBackendPort*>(src)->disconnect (dst_port);
}

int
CoreAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<CoreBackendPort*>(port)->disconnect_all ();
	return 0;
}

bool
CoreAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<CoreBackendPort*>(port)->is_connected ();
}

bool
CoreAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	CoreBackendPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("CoreAudioBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<CoreBackendPort*>(src)->is_connected (dst_port);
}

bool
CoreAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<CoreBackendPort*>(port)->is_physically_connected ();
}

int
CoreAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreAudioBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::vector<CoreBackendPort*>& connected_ports = static_cast<CoreBackendPort*>(port)->get_connections ();

	for (std::vector<CoreBackendPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
CoreAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t** buf, void* port_buffer,
		uint32_t event_index)
{
	assert (buf && port_buffer);
	CoreMidiBuffer& source = * static_cast<CoreMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	CoreMidiEvent * const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size = event->size ();
	*buf = event->data ();
	return 0;
}

int
CoreAudioBackend::midi_event_put (
		void* port_buffer,
		pframes_t timestamp,
		const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	CoreMidiBuffer& dst = * static_cast<CoreMidiBuffer*>(port_buffer);
	if (dst.size () && (pframes_t)dst.back ()->timestamp () > timestamp) {
		fprintf (stderr, "CoreMidiBuffer: it's too late for this event. %d > %d\n",
				(pframes_t)dst.back ()->timestamp (), timestamp);
		return -1;
	}
	dst.push_back (boost::shared_ptr<CoreMidiEvent>(new CoreMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
CoreAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<CoreMidiBuffer*>(port_buffer)->size ();
}

void
CoreAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
	CoreMidiBuffer * buf = static_cast<CoreMidiBuffer*>(port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
CoreAudioBackend::can_monitor_input () const
{
	return false;
}

int
CoreAudioBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
CoreAudioBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
CoreAudioBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
CoreAudioBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreBackendPort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<CoreBackendPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
CoreAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	if (!valid_port (port)) {
		PBD::error << _("CoreBackendPort::get_latency_range (): invalid port.") << endmsg;
		LatencyRange r;
		r.min = 0;
		r.max = 0;
		return r;
	}
	return static_cast<CoreBackendPort*>(port)->latency_range (for_playback);
}

/* Discovering physical ports */

bool
CoreAudioBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("CoreBackendPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<CoreBackendPort*>(port)->is_physical ();
}

void
CoreAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		CoreBackendPort* port = _ports[i];
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
CoreAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		CoreBackendPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
CoreAudioBackend::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		CoreBackendPort* port = _ports[i];
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
CoreAudioBackend::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		CoreBackendPort* port = _ports[i];
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
CoreAudioBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	assert (port);
	assert (valid_port (port));
	return static_cast<CoreBackendPort*>(port)->get_buffer (nframes);
}

void
CoreAudioBackend::post_process ()
{
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

void *
CoreAudioBackend::freewheel_thread ()
{
	_active_fw = true;
	bool first_run = false;
	while (_run) {
		// check if we should run,
		if (_freewheeling != _freewheel) {
			if (!_freewheeling) {
				// handshake w/ coreaudio
				_reinit_thread_callback = true;
				_freewheel_ack = false;
			}

			engine.freewheel_callback (_freewheeling);
			first_run = true;
			_freewheel = _freewheeling;
		}

		if (!_freewheel || !_freewheel_ack) {
			// TODO use a pthread sync/sleep
			Glib::usleep(200000);
			continue;
		}

		if (first_run) {
			first_run = false;
			_main_thread = pthread_self();
			AudioEngine::thread_init_callback (this);
		}

		// Freewheelin'
		for (std::vector<CoreBackendPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
		}
		for (std::vector<CoreBackendPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
			static_cast<CoreMidiBuffer*>((*it)->get_buffer(0))->clear ();
		}

		if (engine.process_callback (_samples_per_period)) {
			break;
		}
		_dsp_load = 1.0;
		Glib::usleep (100); // don't hog cpu

		post_process();
	}

	_active_fw = false;

	if (_run && _freewheel) {
		engine.halted_callback("CoreAudio Freehweeling aborted.");
	}
	return 0;
}

int
CoreAudioBackend::process_callback ()
{
	uint32_t i = 0;
	uint64_t clock1, clock2;

	_active_ca = true;

	if (_run && _freewheel && !_freewheel_ack) {
		_freewheel_ack = true;
	}

	if (!_run || _freewheel || _preinit) {
		return 1;
	}

	if (pthread_mutex_trylock (&_process_callback_mutex)) {
		return 1;
	}

	if (_reinit_thread_callback || _main_thread != pthread_self()) {
		_reinit_thread_callback = false;
		_main_thread = pthread_self();
		AudioEngine::thread_init_callback (this);

		manager.registration_callback();
		manager.graph_order_callback();
	}

	const uint32_t n_samples = _pcmio->n_samples();

#if 0 // here in RT callback ?? XXX
	if (_samples_per_period != n_samples) {
		printf("CoreAudio Adjust SPP %zu -> %d\n", _samples_per_period, n_samples);
		_samples_per_period = n_samples;
		engine.buffer_size_change (_samples_per_period);
		// TODO update latencies
	}
#endif

	// cycle-length in usec
	const int64_t nominal_time = 1e6 * n_samples / _samplerate;

	clock1 = g_get_monotonic_time();

	/* get midi */
	i=0;
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
		CoreMidiBuffer* mbuf = static_cast<CoreMidiBuffer*>((*it)->get_buffer(0));
		mbuf->clear();
		uint64_t time_ns;
		uint8_t data[64]; // match MaxAlsaEventSize in alsa_rawmidi.cc
		size_t size = sizeof(data);
		while (_midiio->recv_event (i, nominal_time, time_ns, data, size)) {
			pframes_t time = floor((float) time_ns * _samplerate * 1e-9);
			assert (time < n_samples);
			midi_event_put((void*)mbuf, time, data, size);
			size = sizeof(data);
		}
	}

	/* get audio */
	i = 0;
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
		_pcmio->get_capture_channel (i, (float*)((*it)->get_buffer(n_samples)), n_samples);
	}

	/* clear output buffers */
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		memset ((*it)->get_buffer (n_samples), 0, n_samples * sizeof (Sample));
	}

	_midiio->start_cycle();

	if (engine.process_callback (n_samples)) {
		fprintf(stderr, "ENGINE PROCESS ERROR\n");
		//_pcmio->pcm_stop ();
		_active_ca = false;
		pthread_mutex_unlock (&_process_callback_mutex);
		return -1;
	}

	/* mixdown midi */
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
		static_cast<CoreMidiPort*>(*it)->get_buffer(0);
	}

	/* queue outgoing midi */
	i = 0;
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
		const CoreMidiBuffer *src = static_cast<const CoreMidiPort*>(*it)->const_buffer();
		for (CoreMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
			_midiio->send_event (i, (*mit)->timestamp() / nominal_time, (*mit)->data(), (*mit)->size());
		}
	}

	/* write back audio */
	i = 0;
	for (std::vector<CoreBackendPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
		_pcmio->set_playback_channel (i, (float const*)(*it)->get_buffer (n_samples), n_samples);
	}

	_processed_samples += n_samples;

	/* calc DSP load. */
	clock2 = g_get_monotonic_time();
	const int64_t elapsed_time = clock2 - clock1;
	_dsp_load = elapsed_time / (float) nominal_time;

	//engine.Xrun (); // TODO, if any

	/* port-connection change */
	post_process();
	pthread_mutex_unlock (&_process_callback_mutex);
	return 0;
}

void
CoreAudioBackend::error_callback ()
{
	_pcmio->set_error_callback (NULL, NULL);
	engine.halted_callback("CoreAudio Process aborted.");
}

void
CoreAudioBackend::hw_changed_callback ()
{
	_reinit_thread_callback = true;
	engine.request_device_list_update();
}

/******************************************************************************/

static boost::shared_ptr<CoreAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	"CoreAudio",
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
		_instance.reset (new CoreAudioBackend (e, _descriptor));
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
CoreBackendPort::CoreBackendPort (CoreAudioBackend &b, const std::string& name, PortFlags flags)
	: _osx_backend (b)
	, _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

CoreBackendPort::~CoreBackendPort () {
	disconnect_all ();
}


int CoreBackendPort::connect (CoreBackendPort *port)
{
	if (!port) {
		PBD::error << _("CoreBackendPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("CoreBackendPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("CoreBackendPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("CoreBackendPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("CoreBackendPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("CoreBackendPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}


void CoreBackendPort::_connect (CoreBackendPort *port, bool callback)
{
	_connections.push_back (port);
	if (callback) {
		port->_connect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), true);
	}
}

int CoreBackendPort::disconnect (CoreBackendPort *port)
{
	if (!port) {
		PBD::error << _("CoreBackendPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("CoreBackendPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void CoreBackendPort::_disconnect (CoreBackendPort *port, bool callback)
{
	std::vector<CoreBackendPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);

	assert (it != _connections.end ());

	_connections.erase (it);

	if (callback) {
		port->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void CoreBackendPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  _connections.back ()->name(), false);
		_connections.pop_back ();
	}
}

bool
CoreBackendPort::is_connected (const CoreBackendPort *port) const
{
	return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
}

bool CoreBackendPort::is_physically_connected () const
{
	for (std::vector<CoreBackendPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

/******************************************************************************/

CoreAudioPort::CoreAudioPort (CoreAudioBackend &b, const std::string& name, PortFlags flags)
	: CoreBackendPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock(_buffer, sizeof (_buffer));
}

CoreAudioPort::~CoreAudioPort () { }

void* CoreAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<CoreBackendPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			CoreAudioPort const * source = static_cast<const CoreAudioPort*>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<const CoreAudioPort*>(*it);
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


CoreMidiPort::CoreMidiPort (CoreAudioBackend &b, const std::string& name, PortFlags flags)
	: CoreBackendPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();
}

CoreMidiPort::~CoreMidiPort () { }

struct MidiEventSorter {
	bool operator() (const boost::shared_ptr<CoreMidiEvent>& a, const boost::shared_ptr<CoreMidiEvent>& b) {
		return *a < *b;
	}
};

void* CoreMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		for (std::vector<CoreBackendPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const CoreMidiBuffer * src = static_cast<const CoreMidiPort*>(*i)->const_buffer ();
			for (CoreMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (boost::shared_ptr<CoreMidiEvent>(new CoreMidiEvent (**it)));
			}
		}
		std::sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}
	return &(_buffer[_bufperiod]);
}

CoreMidiEvent::CoreMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
	, _data (0)
{
	if (size > 0) {
		_data = (uint8_t*) malloc (size);
		memcpy (_data, data, size);
	}
}

CoreMidiEvent::CoreMidiEvent (const CoreMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
	, _data (0)
{
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

CoreMidiEvent::~CoreMidiEvent () {
	free (_data);
};
