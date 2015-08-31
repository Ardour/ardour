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

#include "win_utils.h"
#include "mmcss.h"
#include "audio_utils.h"

#include "debug.h"

using namespace ARDOUR;

namespace {

const char * const winmme_driver_name = X_("WinMME");

}

static std::string s_instance_name;
size_t PortAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> PortAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> PortAudioBackend::_input_audio_device_status;
std::vector<AudioBackend::DeviceStatus> PortAudioBackend::_output_audio_device_status;

PortAudioBackend::PortAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _pcmio (0)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _measure_latency (false)
	, m_cycle_count(0)
	, m_total_deviation_us(0)
	, m_max_deviation_us(0)
	, _input_audio_device("")
	, _output_audio_device("")
	, _midi_driver_option(get_standard_device_name(DeviceNone))
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

	mmcss::initialize ();

	_pcmio = new PortAudioIO ();
	_midiio = new WinMMEMidiIO ();
}

PortAudioBackend::~PortAudioBackend ()
{
	delete _pcmio; _pcmio = 0;
	delete _midiio; _midiio = 0;

	mmcss::deinitialize ();

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

bool
PortAudioBackend::requires_driver_selection() const
{
	// we could do this but implementation would need changing
	/*
	if (enumerate_drivers().size() == 1) {
		return false;
	}
	*/
	return true;
}

std::vector<std::string>
PortAudioBackend::enumerate_drivers () const
{
	DEBUG_AUDIO ("Portaudio: enumerate_drivers\n");
	std::vector<std::string> currently_available;
	_pcmio->host_api_list (currently_available);
	return currently_available;
}

int
PortAudioBackend::set_driver (const std::string& name)
{
	DEBUG_AUDIO (string_compose ("Portaudio: set_driver %1 \n", name));
	if (!_pcmio->set_host_api (name)) {
		DEBUG_AUDIO (string_compose ("Portaudio: Unable to set_driver %1 \n", name));
		return -1;
	}
	_pcmio->update_devices();
	return 0;
}

bool
PortAudioBackend::update_devices ()
{
	return _pcmio->update_devices();
}

std::string
PortAudioBackend::driver_name () const
{
	std::string driver_name = _pcmio->get_host_api ();
	DEBUG_AUDIO (string_compose ("Portaudio: driver_name %1 \n", driver_name));
	return driver_name;
}

bool
PortAudioBackend::use_separate_input_and_output_devices () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
PortAudioBackend::enumerate_devices () const
{
	DEBUG_AUDIO ("Portaudio: ERROR enumerate devices should not be called \n");
	return std::vector<AudioBackend::DeviceStatus>();
}

std::vector<AudioBackend::DeviceStatus>
PortAudioBackend::enumerate_input_devices () const
{
	_input_audio_device_status.clear();
	std::map<int, std::string> input_devices;
	_pcmio->input_device_list(input_devices);

	for (std::map<int, std::string>::const_iterator i = input_devices.begin (); i != input_devices.end(); ++i) {
		if (_input_audio_device == "") _input_audio_device = i->second;
		_input_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _input_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
PortAudioBackend::enumerate_output_devices () const
{
	_output_audio_device_status.clear();
	std::map<int, std::string> output_devices;
	_pcmio->output_device_list(output_devices);

	for (std::map<int, std::string>::const_iterator i = output_devices.begin (); i != output_devices.end(); ++i) {
		if (_output_audio_device == "") _output_audio_device = i->second;
		_output_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _output_audio_device_status;
}

std::vector<float>
PortAudioBackend::available_sample_rates (const std::string&) const
{
	DEBUG_AUDIO ("Portaudio: available_sample_rates\n");
	std::vector<float> sr;
	_pcmio->available_sample_rates(name_to_id(_input_audio_device), sr);
	return sr;
}

std::vector<uint32_t>
PortAudioBackend::available_buffer_sizes (const std::string&) const
{
	DEBUG_AUDIO ("Portaudio: available_buffer_sizes\n");
	std::vector<uint32_t> bs;
	_pcmio->available_buffer_sizes(name_to_id(_input_audio_device), bs);
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
	DEBUG_AUDIO ("Portaudio: set_device_name should not be called\n");
	return 0;
}

int
PortAudioBackend::set_input_device_name (const std::string& d)
{
	DEBUG_AUDIO (string_compose ("Portaudio: set_input_device_name %1\n", d));
	_input_audio_device = d;
	return 0;
}

int
PortAudioBackend::set_output_device_name (const std::string& d)
{
	DEBUG_AUDIO (string_compose ("Portaudio: set_output_device_name %1\n", d));
	_output_audio_device = d;
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
	return "Unused";
}

std::string
PortAudioBackend::input_device_name () const
{
	return _input_audio_device;
}

std::string
PortAudioBackend::output_device_name () const
{
	return _output_audio_device;
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

std::string
PortAudioBackend::control_app_name () const
{
	return _pcmio->control_app_name (name_to_id (_input_audio_device));
}

void
PortAudioBackend::launch_control_app ()
{
	return _pcmio->launch_control_app (name_to_id(_input_audio_device));
}

/* MIDI */

std::vector<std::string>
PortAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		_midi_options.push_back (winmme_driver_name);
		_midi_options.push_back (get_standard_device_name(DeviceNone));
	}
	return _midi_options;
}

int
PortAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != get_standard_device_name(DeviceNone) && opt != winmme_driver_name) {
		return -1;
	}
	DEBUG_MIDI (string_compose ("Setting midi option to %1\n", opt));
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
	d->main_blocking_process_thread ();
	pthread_exit (0);
	return 0;
}

bool
PortAudioBackend::engine_halted ()
{
	return !_active && _run;
}

bool
PortAudioBackend::running ()
{
	return _active || _run;
}

int
PortAudioBackend::_start (bool for_latency_measurement)
{
	if (engine_halted()) {
		stop();
	}

	if (running()) {
		DEBUG_AUDIO("Already started.\n");
		return -1;
	}

	if (_ports.size()) {
		DEBUG_AUDIO(
		    "Recovering from unclean shutdown, port registry is not empty.\n");
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

	PortAudioIO::ErrorCode err;

	err = _pcmio->open_blocking_stream(name_to_id(_input_audio_device),
	                                   name_to_id(_output_audio_device),
	                                   _samplerate,
	                                   _samples_per_period);

	switch (err) {
	case PortAudioIO::NoError:
		break;
	case PortAudioIO::DeviceConfigNotSupportedError:
		PBD::error << get_error_string(DeviceConfigurationNotSupportedError)
		           << endmsg;
		return -1;
	default:
		PBD::error << get_error_string(AudioDeviceOpenError) << endmsg;
		return -1;
	}

	if (_n_outputs != _pcmio->n_playback_channels ()) {
		_n_outputs = _pcmio->n_playback_channels ();
		PBD::info << get_error_string(OutputChannelCountNotSupportedError) << endmsg;
	}

	if (_n_inputs != _pcmio->n_capture_channels ()) {
		_n_inputs = _pcmio->n_capture_channels ();
		PBD::info << get_error_string(InputChannelCountNotSupportedError) << endmsg;
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
		PBD::warning << get_error_string(SampleRateNotSupportedError) << endmsg;
	}

	_measure_latency = for_latency_measurement;

	_run = true;
	_port_change_flag = false;

	if (_midi_driver_option == winmme_driver_name) {
		_midiio->set_enabled(true);
		//_midiio->set_port_changed_callback(midi_port_change, this);
		_midiio->start(); // triggers port discovery, callback coremidi_rediscover()
	}

	m_cycle_timer.set_samplerate(_samplerate);
	m_cycle_timer.set_samples_per_cycle(_samples_per_period);

	m_dsp_calc.set_max_time_us (m_cycle_timer.get_length_us());

	DEBUG_MIDI ("Registering MIDI ports\n");

	if (register_system_midi_ports () != 0) {
		DEBUG_PORTS("Failed to register system midi ports.\n")
		_run = false;
		return -1;
	}

	DEBUG_AUDIO ("Registering Audio ports\n");

	if (register_system_audio_ports()) {
		DEBUG_PORTS("Failed to register system audio ports.\n");
		_run = false;
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		DEBUG_PORTS("Could not re-establish ports.\n");
		_run = false;
		return -1;
	}

	engine.reconnect_ports ();
	_run = true;
	_port_change_flag = false;

	if (!start_blocking_process_thread()) {
		return -1;
	}

	return 0;
}

bool
PortAudioBackend::start_blocking_process_thread ()
{
	if (_realtime_pthread_create (SCHED_FIFO, -20, 100000,
				&_main_blocking_thread, pthread_process, this))
	{
		if (pthread_create (&_main_blocking_thread, NULL, pthread_process, this))
		{
			DEBUG_AUDIO("Failed to create main audio thread\n");
			_run = false;
			return false;
		} else {
			PBD::warning << get_error_string(AquireRealtimePermissionError) << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_active) {
		DEBUG_AUDIO("Failed to start main audio thread\n");
		_pcmio->close_stream();
		_run = false;
		unregister_ports();
		_active = false;
		return false;
	}
	return true;
}

bool
PortAudioBackend::stop_blocking_process_thread ()
{
	void *status;

	if (pthread_join (_main_blocking_thread, &status)) {
		DEBUG_AUDIO("Failed to stop main audio thread\n");
		return false;
	}

	return true;
}

int
PortAudioBackend::stop ()
{
	if (!_run) {
		return 0;
	}

	_midiio->stop();

	_run = false;

	if (!stop_blocking_process_thread ()) {
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
	if (!m_cycle_timer.valid()) {
		return 0;
	}

	return m_cycle_timer.samples_since_cycle_start (utils::get_microseconds());
}

int
PortAudioBackend::name_to_id(std::string device_name) const {
	uint32_t device_id = UINT32_MAX;
	std::map<int, std::string> devices;
	_pcmio->input_device_list(devices);
	_pcmio->output_device_list(devices);

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

#ifdef USE_MMCSS_THREAD_PRIORITIES
	HANDLE task_handle;

	mmcss::set_thread_characteristics ("Pro Audio", &task_handle);
	if (!mmcss::set_thread_priority(task_handle, mmcss::AVRT_PRIORITY_NORMAL)) {
		PBD::warning << get_error_string(SettingAudioThreadPriorityError)
		             << endmsg;
	}
#endif

	DWORD tid = GetCurrentThreadId ();
	DEBUG_THREADS (string_compose ("Process Thread Child ID: %1\n", tid));

	f ();

#ifdef USE_MMCSS_THREAD_PRIORITIES
	mmcss::revert_thread_characteristics (task_handle);
#endif

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
			DEBUG_AUDIO("Cannot create process thread.");
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
			DEBUG_AUDIO("Cannot terminate process thread.");
			rv -= 1;
		}
	}
	_threads.clear ();
	return rv;
}

bool
PortAudioBackend::in_process_thread ()
{
	if (pthread_equal (_main_blocking_thread, pthread_self()) != 0) {
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
		DEBUG_PORTS("set_port_name: Invalid Port(s)\n");
		return -1;
	}
	return static_cast<PamPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
PortAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		DEBUG_PORTS("get_port_name: Invalid Port(s)\n");
		return std::string ();
	}
	return static_cast<PamPort*>(port)->name ();
}

int
PortAudioBackend::get_port_property (PortHandle port,
                                     const std::string& key,
                                     std::string& value,
                                     std::string& type) const
{
	if (!valid_port (port)) {
		DEBUG_PORTS("get_port_name: Invalid Port(s)\n");
		return -1;
	}

	if (key == "http://jackaudio.org/metadata/pretty-name") {
		type = "";
		value = static_cast<PamPort*>(port)->pretty_name ();
		if (!value.empty()) {
			return 0;
		}
	}
	return -1;
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
		DEBUG_PORTS(
		    string_compose("register_port: Port already exists: (%1)\n", name));
		return 0;
	}
	PamPort* port = NULL;
	switch (type) {
	case DataType::AUDIO:
		port = new PortAudioPort(*this, name, flags);
		break;
	case DataType::MIDI:
		port = new PortMidiPort(*this, name, flags);
		break;
	default:
		DEBUG_PORTS("register_port: Invalid Data Type.\n");
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
		DEBUG_PORTS("unregister_port: Failed to find port\n");
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
		PortAudioPort* audio_port = static_cast<PortAudioPort*>(p);
		audio_port->set_pretty_name (
		    _pcmio->get_input_channel_name (name_to_id (_input_audio_device), i));
		_system_inputs.push_back (audio_port);
	}

	lr.min = lr.max = portaudio_reported_output_latency + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (uint32_t i = 0; i < a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		PortAudioPort* audio_port = static_cast<PortAudioPort*>(p);
		audio_port->set_pretty_name (
		    _pcmio->get_output_channel_name (name_to_id (_output_audio_device), i));
		_system_outputs.push_back(audio_port);
	}
	return 0;
}

int
PortAudioBackend::register_system_midi_ports()
{
	if (_midi_driver_option == get_standard_device_name(DeviceNone)) {
		DEBUG_MIDI("No MIDI backend selected, not system midi ports available\n");
		return 0;
	}

	LatencyRange lr;
	lr.min = lr.max = _samples_per_period;

	const std::vector<WinMMEMidiInputDevice*> inputs = _midiio->get_inputs();

	for (std::vector<WinMMEMidiInputDevice*>::const_iterator i = inputs.begin ();
	     i != inputs.end ();
	     ++i) {
		std::string port_name = "system_midi:" + (*i)->name() + " capture";
		PortHandle p =
		    add_port (port_name,
		              DataType::MIDI,
		              static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		PortMidiPort* midi_port = static_cast<PortMidiPort*>(p);
		midi_port->set_pretty_name ((*i)->name());
		_system_midi_in.push_back (midi_port);
		DEBUG_MIDI (string_compose ("Registered MIDI input port: %1\n", port_name));
	}

	const std::vector<WinMMEMidiOutputDevice*> outputs = _midiio->get_outputs();

	for (std::vector<WinMMEMidiOutputDevice*>::const_iterator i = outputs.begin ();
	     i != outputs.end ();
	     ++i) {
		std::string port_name = "system_midi:" + (*i)->name() + " playback";
		PortHandle p =
		    add_port (port_name,
		              DataType::MIDI,
		              static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		PortMidiPort* midi_port = static_cast<PortMidiPort*>(p);
		midi_port->set_n_periods(2);
		midi_port->set_pretty_name ((*i)->name());
		_system_midi_out.push_back (midi_port);
		DEBUG_MIDI (string_compose ("Registered MIDI output port: %1\n", port_name));
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
		DEBUG_PORTS(string_compose("connect: Invalid Source port: (%1)\n", src));
		return -1;
	}
	if (!dst_port) {
		DEBUG_PORTS(string_compose("connect: Invalid Destination port: (%1)\n", dst));
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
		DEBUG_PORTS("disconnect: Invalid Port(s)\n");
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
PortAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		DEBUG_PORTS("connect: Invalid Source Port Handle\n");
		return -1;
	}
	if (!dst_port) {
		DEBUG_PORTS(string_compose("connect: Invalid Destination Port (%1)\n", dst));
		return -1;
	}
	return static_cast<PamPort*>(src)->connect (dst_port);
}

int
PortAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		DEBUG_PORTS("disconnect: Invalid Port(s)\n");
		return -1;
	}
	return static_cast<PamPort*>(src)->disconnect (dst_port);
}

int
PortAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		DEBUG_PORTS("disconnect_all: Invalid Port\n");
		return -1;
	}
	static_cast<PamPort*>(port)->disconnect_all ();
	return 0;
}

bool
PortAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		DEBUG_PORTS("disconnect_all: Invalid Port\n");
		return false;
	}
	return static_cast<PamPort*>(port)->is_connected ();
}

bool
PortAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	PamPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		DEBUG_PORTS("connected_to: Invalid Port\n");
		return false;
	}
	return static_cast<PamPort*>(src)->is_connected (dst_port);
}

bool
PortAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		DEBUG_PORTS("physically_connected: Invalid Port\n");
		return false;
	}
	return static_cast<PamPort*>(port)->is_physically_connected ();
}

int
PortAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		DEBUG_PORTS("get_connections: Invalid Port\n");
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
		// nevermind, ::get_buffer() sorts events
		DEBUG_MIDI (string_compose ("PortMidiBuffer: unordered event: %1 > %2\n",
		                            (pframes_t)dst.back ()->timestamp (),
		                            timestamp));
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
		DEBUG_PORTS("PamPort::set_latency_range (): invalid port.\n");
	}
	static_cast<PamPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
PortAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	LatencyRange r;
	if (!valid_port (port)) {
		DEBUG_PORTS("PamPort::get_latency_range (): invalid port.\n");
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
		DEBUG_PORTS("PamPort::port_is_physical (): invalid port.\n");
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
			case DataType::AUDIO:
				++n_audio;
				break;
			case DataType::MIDI:
				++n_midi;
				break;
			default:
				break;
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
			case DataType::AUDIO:
				++n_audio;
				break;
			case DataType::MIDI:
				++n_midi;
				break;
			default:
				break;
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
PortAudioBackend::main_blocking_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active = true;
	_processed_samples = 0;

	manager.registration_callback();
	manager.graph_order_callback();

	if (_pcmio->start_stream() != PortAudioIO::NoError) {
		_pcmio->close_stream ();
		_active = false;
		engine.halted_callback(get_error_string(AudioDeviceIOError).c_str());
	}

#ifdef USE_MMCSS_THREAD_PRIORITIES
	HANDLE task_handle;

	mmcss::set_thread_characteristics ("Pro Audio", &task_handle);
	if (!mmcss::set_thread_priority(task_handle, mmcss::AVRT_PRIORITY_NORMAL)) {
		PBD::warning << get_error_string(SettingAudioThreadPriorityError)
		             << endmsg;
	}
#endif

	DWORD tid = GetCurrentThreadId ();
	DEBUG_THREADS (string_compose ("Process Thread Master ID: %1\n", tid));

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
				DEBUG_AUDIO("PortAudio: Xrun\n");
				engine.Xrun();
				break;
			default:
				PBD::error << get_error_string(AudioDeviceIOError) << endmsg;
				break;
			}

			if (!blocking_process_main(_pcmio->get_capture_buffer(),
			                           _pcmio->get_playback_buffer())) {
				return 0;
			}
		} else {

			if (!blocking_process_freewheel()) {
				return 0;
			}
		}

		process_port_connection_changes();
	}
	_pcmio->close_stream();
	_active = false;
	if (_run) {
		engine.halted_callback(get_error_string(AudioDeviceIOError).c_str());
	}

#ifdef USE_MMCSS_THREAD_PRIORITIES
	mmcss::revert_thread_characteristics (task_handle);
#endif

	return 0;
}

bool
PortAudioBackend::blocking_process_main(const float* interleaved_input_data,
                                        float* interleaved_output_data)
{
	uint32_t i = 0;
	uint64_t min_elapsed_us = 1000000;
	uint64_t max_elapsed_us = 0;

	m_dsp_calc.set_start_timestamp_us (utils::get_microseconds());

	i = 0;
	/* Copy input audio data into input port buffers */
	for (std::vector<PamPort*>::const_iterator it = _system_inputs.begin();
	     it != _system_inputs.end();
	     ++it, ++i) {
		assert(_system_inputs.size() == _pcmio->n_capture_channels());
		uint32_t channels = _system_inputs.size();
		float* input_port_buffer = (float*)(*it)->get_buffer(_samples_per_period);
		deinterleave_audio_data(
		    interleaved_input_data, input_port_buffer, _samples_per_period, i, channels);
	}

	process_incoming_midi ();

	/* clear output buffers */
	for (std::vector<PamPort*>::const_iterator it = _system_outputs.begin();
	     it != _system_outputs.end();
	     ++it) {
		memset((*it)->get_buffer(_samples_per_period),
		       0,
		       _samples_per_period * sizeof(Sample));
	}

	m_last_cycle_start = m_cycle_timer.get_start();
	m_cycle_timer.reset_start(utils::get_microseconds());
	m_cycle_count++;

	uint64_t cycle_diff_us = (m_cycle_timer.get_start() - m_last_cycle_start);
	int64_t deviation_us = (cycle_diff_us - m_cycle_timer.get_length_us());
	m_total_deviation_us += ::llabs(deviation_us);
	m_max_deviation_us =
	    std::max(m_max_deviation_us, (uint64_t)::llabs(deviation_us));

	if ((m_cycle_count % 1000) == 0) {
		uint64_t mean_deviation_us = m_total_deviation_us / m_cycle_count;
		DEBUG_TIMING(string_compose("Mean avg cycle deviation: %1(ms), max %2(ms)\n",
		                            mean_deviation_us * 1e-3,
		                            m_max_deviation_us * 1e-3));
	}

	if (::llabs(deviation_us) > m_cycle_timer.get_length_us()) {
		DEBUG_TIMING(
		    string_compose("time between process(ms): %1, Est(ms): %2, Dev(ms): %3\n",
		                   cycle_diff_us * 1e-3,
		                   m_cycle_timer.get_length_us() * 1e-3,
		                   deviation_us * 1e-3));
	}

	/* call engine process callback */
	if (engine.process_callback(_samples_per_period)) {
		_pcmio->close_stream();
		_active = false;
		return false;
	}

	process_outgoing_midi ();

	/* write back audio */
	i = 0;
	for (std::vector<PamPort*>::const_iterator it = _system_outputs.begin();
	     it != _system_outputs.end();
	     ++it, ++i) {
		assert(_system_outputs.size() == _pcmio->n_playback_channels());
		const uint32_t channels = _system_outputs.size();
		float* output_port_buffer = (float*)(*it)->get_buffer(_samples_per_period);
		interleave_audio_data(
		    output_port_buffer, interleaved_output_data, _samples_per_period, i, channels);
	}

	_processed_samples += _samples_per_period;

	/* calculate DSP load */
	m_dsp_calc.set_stop_timestamp_us (utils::get_microseconds());
	_dsp_load = m_dsp_calc.get_dsp_load();

	DEBUG_TIMING(string_compose("DSP Load: %1\n", _dsp_load));

	max_elapsed_us = std::max(m_dsp_calc.elapsed_time_us(), max_elapsed_us);
	min_elapsed_us = std::min(m_dsp_calc.elapsed_time_us(), min_elapsed_us);
	if ((m_cycle_count % 1000) == 0) {
		DEBUG_TIMING(string_compose("Elapsed process time(usecs) max: %1, min: %2\n",
		                            max_elapsed_us,
		                            min_elapsed_us));
	}

	return true;
}

bool
PortAudioBackend::blocking_process_freewheel()
{
	// zero audio input buffers
	for (std::vector<PamPort*>::const_iterator it = _system_inputs.begin();
	     it != _system_inputs.end();
	     ++it) {
		memset((*it)->get_buffer(_samples_per_period),
		       0,
		       _samples_per_period * sizeof(Sample));
	}

	// TODO clear midi or stop midi recv when entering fwheelin'

	if (engine.process_callback(_samples_per_period)) {
		_pcmio->close_stream();
		_active = false;
		return false;
	}

	// drop all outgoing MIDI messages
	for (std::vector<PamPort*>::const_iterator it = _system_midi_out.begin();
	     it != _system_midi_out.end();
	     ++it) {
		void* bptr = (*it)->get_buffer(0);
		midi_clear(bptr);
	}

	_dsp_load = 1.0;
	Glib::usleep(100); // don't hog cpu
	return true;
}

void
PortAudioBackend::process_incoming_midi ()
{
	uint32_t i = 0;
	for (std::vector<PamPort*>::const_iterator it = _system_midi_in.begin();
	     it != _system_midi_in.end();
	     ++it, ++i) {
		PortMidiBuffer* mbuf = static_cast<PortMidiBuffer*>((*it)->get_buffer(0));
		mbuf->clear();
		uint64_t timestamp;
		pframes_t sample_offset;
		uint8_t data[256];
		size_t size = sizeof(data);
		while (_midiio->dequeue_input_event(i,
		                                    m_cycle_timer.get_start(),
		                                    m_cycle_timer.get_next_start(),
		                                    timestamp,
		                                    data,
		                                    size)) {
			sample_offset = m_cycle_timer.samples_since_cycle_start(timestamp);
			midi_event_put(mbuf, sample_offset, data, size);
			DEBUG_MIDI(string_compose("Dequeuing incoming MIDI data for device: %1 "
			                          "sample_offset: %2 timestamp: %3, size: %4\n",
			                          _midiio->get_inputs()[i]->name(),
			                          sample_offset,
			                          timestamp,
			                          size));
			size = sizeof(data);
		}
	}
}

void
PortAudioBackend::process_outgoing_midi ()
{
	/* mixdown midi */
	for (std::vector<PamPort*>::iterator it = _system_midi_out.begin();
	     it != _system_midi_out.end();
	     ++it) {
		static_cast<PortMidiPort*>(*it)->next_period();
	}
	/* queue outgoing midi */
	uint32_t i = 0;
	for (std::vector<PamPort*>::const_iterator it = _system_midi_out.begin();
	     it != _system_midi_out.end();
	     ++it, ++i) {
		const PortMidiBuffer* src =
		    static_cast<const PortMidiPort*>(*it)->const_buffer();

		for (PortMidiBuffer::const_iterator mit = src->begin(); mit != src->end();
		     ++mit) {
			uint64_t timestamp =
			    m_cycle_timer.timestamp_from_sample_offset((*mit)->timestamp());
			DEBUG_MIDI(string_compose("Queuing outgoing MIDI data for device: "
			                          "%1 sample_offset: %2 timestamp: %3, size: %4\n",
			                          _midiio->get_outputs()[i]->name(),
			                          (*mit)->timestamp(),
			                          timestamp,
			                          (*mit)->size()));
			_midiio->enqueue_output_event(i, timestamp, (*mit)->data(), (*mit)->size());
		}
	}
}

void
PortAudioBackend::process_port_connection_changes ()
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
		DEBUG_PORTS("PamPort::connect (): invalid (null) port\n");
		return -1;
	}

	if (type () != port->type ()) {
		DEBUG_PORTS("PamPort::connect (): wrong port-type\n");
		return -1;
	}

	if (is_output () && port->is_output ()) {
		DEBUG_PORTS("PamPort::connect (): cannot inter-connect output ports.\n");
		return -1;
	}

	if (is_input () && port->is_input ()) {
		DEBUG_PORTS("PamPort::connect (): cannot inter-connect input ports.\n");
		return -1;
	}

	if (this == port) {
		DEBUG_PORTS("PamPort::connect (): cannot self-connect ports.\n");
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
		DEBUG_PORTS("PamPort::disconnect (): invalid (null) port\n");
		return -1;
	}

	if (!is_connected (port)) {
		DEBUG_PORTS(string_compose(
		    "PamPort::disconnect (): ports are not connected: (%1) -> (%2)\n",
		    name(),
		    port->name()));
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
