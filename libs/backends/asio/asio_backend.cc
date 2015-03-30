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

#include "asio_backend.h"
#include "rt_thread.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t ASIOBackend::_max_buffer_size = 8192;
std::vector<std::string> ASIOBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> ASIOBackend::_audio_device_status;


/* static class instance access */
static void hw_changed_callback_ptr (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	d->hw_changed_callback();
}

static void error_callback_ptr (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	d->error_callback();
}

static void xrun_callback_ptr (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	d->xrun_callback();
}

static void buffer_size_callback_ptr (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	d->buffer_size_callback();
}

static void sample_rate_callback_ptr (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	d->sample_rate_callback();
}


ASIOBackend::ASIOBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _run (false)
	, _active_cb (false)
	, _active_fw (false)
	, _freewheeling (false)
	, _freewheel (false)
	, _freewheel_ack (false)
	, _reinit_thread_callback (false)
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
	pthread_mutex_init (&_process_callback_mutex, 0);
	pthread_mutex_init (&_freewheel_mutex, 0);
	pthread_cond_init  (&_freewheel_signal, 0);

#if 0 // TODO
	//_pcmio = new ASIOPCM ();
	//_midiio = new WindowsMidiIo ();

	_pcmio->set_hw_changed_callback (hw_changed_callback_ptr, this);
	_pcmio->discover();
#endif
}

ASIOBackend::~ASIOBackend ()
{
#if 0 
	delete _pcmio; _pcmio = 0; // TODO
	delete _midiio; _midiio = 0; // TODO
#endif
	pthread_mutex_destroy (&_port_callback_mutex);
	pthread_mutex_destroy (&_process_callback_mutex);
	pthread_mutex_destroy (&_freewheel_mutex);
	pthread_cond_destroy  (&_freewheel_signal);
}

/* AUDIOBACKEND API */

std::string
ASIOBackend::name () const
{
	return X_("ASIO");
}

bool
ASIOBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
ASIOBackend::enumerate_devices () const
{
	_audio_device_status.clear();
#if 0 // TODO
	std::map<size_t, std::string> devices;
	_pcmio->device_list(devices);

	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_audio_device == "") _audio_device = i->second;
		_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
#endif
	return _audio_device_status;
}

std::vector<float>
ASIOBackend::available_sample_rates (const std::string&) const
{
	std::vector<float> sr;
#if 0 // TODO
	_pcmio->available_sample_rates(_audio_device, sr);
#endif
	return sr;
}

std::vector<uint32_t>
ASIOBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
#if 0 // TODO
	_pcmio->available_buffer_sizes(_audio_device, bs);
#endif
	return bs;
}

uint32_t
ASIOBackend::available_input_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

uint32_t
ASIOBackend::available_output_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

bool
ASIOBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
ASIOBackend::can_change_buffer_size_when_running () const
{
	return true;
}

int
ASIOBackend::set_device_name (const std::string& d)
{
	_audio_device = d;
#if 0 // TODO
	const float sr = _pcmio->current_sample_rate(_audio_device);
#endif
	//if (sr > 0) { set_sample_rate(sr); }
	return 0;
}

int
ASIOBackend::set_sample_rate (float sr)
{
	if (sr <= 0) { return -1; }
	// TODO check if it's in the list of valid SR
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
ASIOBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;
#if 0 // TODO
	_pcmio->set_samples_per_period(bs);
#endif
	engine.buffer_size_change (bs);
	return 0;
}

int
ASIOBackend::set_interleaved (bool yn)
{
	if (!yn) { return 0; }
	return -1;
}

int
ASIOBackend::set_input_channels (uint32_t cc)
{
	_n_inputs = cc;
	return 0;
}

int
ASIOBackend::set_output_channels (uint32_t cc)
{
	_n_outputs = cc;
	return 0;
}

int
ASIOBackend::set_systemic_input_latency (uint32_t sl)
{
	_systemic_audio_input_latency = sl;
	return 0;
}

int
ASIOBackend::set_systemic_output_latency (uint32_t sl)
{
	_systemic_audio_output_latency = sl;
	return 0;
}

/* Retrieving parameters */
std::string
ASIOBackend::device_name () const
{
	return _audio_device;
}

float
ASIOBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
ASIOBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
ASIOBackend::interleaved () const
{
	return false;
}

uint32_t
ASIOBackend::input_channels () const
{
	return _n_inputs;
}

uint32_t
ASIOBackend::output_channels () const
{
	return _n_outputs;
}

uint32_t
ASIOBackend::systemic_input_latency () const
{
	return _systemic_audio_input_latency;
}

uint32_t
ASIOBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

/* MIDI */

std::vector<std::string>
ASIOBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		_midi_options.push_back (_("WindowsMidi"));
		_midi_options.push_back (_("None"));
	}
	return _midi_options;
}

int
ASIOBackend::set_midi_option (const std::string& opt)
{
	if (opt != _("None") && opt != _("WindowsMidi")) {
		return -1;
	}
	_midi_driver_option = opt;
	return 0;
}

std::string
ASIOBackend::midi_option () const
{
	return _midi_driver_option;
}

void
ASIOBackend::launch_control_app ()
{
	// TODO
}

/* State Control */

static void * pthread_freewheel (void *arg)
{
	ASIOBackend *d = static_cast<ASIOBackend *>(arg);
	d->freewheel_thread ();
	pthread_exit (0);
	return 0;
}

static int process_callback_ptr (void *arg, const uint32_t n_samples, const uint64_t host_time)
{
	ASIOBackend *d = static_cast<ASIOBackend*> (arg);
	return d->process_callback(n_samples, host_time);
}

int
ASIOBackend::_start (bool for_latency_measurement)
{
	if ((!_active_cb || !_active_fw)  && _run) {
		// recover from 'halted', reap threads
		stop();
	}

	if (_active_cb || _active_fw || _run) {
		PBD::error << _("ASIOBackend: already active.") << endmsg;
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("ASIOBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_system_inputs.clear();
		_system_outputs.clear();
		_system_midi_in.clear();
		_system_midi_out.clear();
		_ports.clear();
	}

	assert(_active_cb == false);
	assert(_active_fw == false);

	_freewheel_ack = false;
	_reinit_thread_callback = true;
	_last_process_start = 0;

#if 0 // TODO
	_pcmio->set_error_callback (error_callback_ptr, this);
	_pcmio->set_buffer_size_callback (buffer_size_callback_ptr, this);
	_pcmio->set_sample_rate_callback (sample_rate_callback_ptr, this);

	_pcmio->pcm_start (device1, device2, _samplerate, _samples_per_period, process_callback_ptr, this);

	switch (_pcmio->state ()) {
		case 0: /* OK */ break;
		case -1: PBD::error << _("ASIOBackend: failed to open device.") << endmsg; break;
		default: PBD::error << _("ASIOBackend: initialization failed.") << endmsg; break;
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
		PBD::info << _("ASIOBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmio->n_capture_channels ()) {
		if (_n_inputs == 0) {
		 _n_inputs = _pcmio->n_capture_channels ();
		} else {
		 _n_inputs = std::min (_n_inputs, _pcmio->n_capture_channels ());
		}
		PBD::info << _("ASIOBackend: adjusted input channel count to match device.") << endmsg;
	}

	if (_pcmio->samples_per_period() != _samples_per_period) {
		_samples_per_period = _pcmio->samples_per_period();
		PBD::warning << _("ASIOBackend: samples per period does not match.") << endmsg;
	}

	if (_pcmio->sample_rate() != _samplerate) {
		_samplerate = _pcmio->sample_rate();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("ASIOBackend: sample rate does not match.") << endmsg;
	}
#endif

	_measure_latency = for_latency_measurement;

	_preinit = true;
	_run = true;
	_port_change_flag = false;

#if 0 // TODO
	if (_midi_driver_option == _("WindowsMidi")) {
		// TODO
		//_midiio->set_enabled(true);
		//_midiio->set_port_changed_callback(midi_port_change, this);
		//_midiio->start(); // triggers port discovery, callback coremidi_rediscover()
	}
#endif

	if (register_system_audio_ports()) {
		PBD::error << _("ASIOBackend: failed to register system ports.") << endmsg;
		_run = false;
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("ASIOBackend: Could not re-establish ports.") << endmsg;
		_run = false;
		return -1;
	}

	if (pthread_create (&_freeewheel_thread, NULL, pthread_freewheel, this))
	{
		PBD::error << _("ASIOBackend: failed to create process thread.") << endmsg;
#if 0 // TODO
		delete _pcmio; _pcmio = 0;
#endif
		_run = false;
		return -1;
	}

	int timeout = 5000;
	while ((!_active_cb || !_active_fw) && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0) {
		PBD::error << _("ASIOBackend: failed to start.") << endmsg;
	}

	if (!_active_fw) {
		PBD::error << _("ASIOBackend: failed to start freewheeling thread.") << endmsg;
		_run = false;
#if 0 // TODO
		_pcmio->pcm_stop();
#endif
		unregister_ports();
		_active_cb = false;
		_active_fw = false;
		return -1;
	}

	if (!_active_cb) {
		PBD::error << _("ASIOBackend: failed to start ASIO.") << endmsg;
		stop();
		_run = false;
		return -1;
	}

	engine.reconnect_ports ();

	// force  an initial registration_callback() & latency re-compute
	_port_change_flag = true;
	pre_process ();

	// all systems go.
#if 0 // TODO
	_pcmio->set_xrun_callback (xrun_callback_ptr, this);
#endif
	_preinit = false;

	return 0;
}

int
ASIOBackend::stop ()
{
	void *status;
	if (!_run) {
		return 0;
	}

	_run = false;
#if 0 // TODO
	_pcmio->pcm_stop();
	_midiio->set_port_changed_callback(NULL, NULL);
	_midiio->stop();
#endif
	pthread_mutex_lock (&_freewheel_mutex);
	pthread_cond_signal (&_freewheel_signal);
	pthread_mutex_unlock (&_freewheel_mutex);

	if (pthread_join (_freeewheel_thread, &status)) {
		PBD::error << _("ASIOBackend: failed to terminate.") << endmsg;
		return -1;
	}

	unregister_ports();

	_active_cb = false;
	_active_fw = false; // ??

	return 0;
}

int
ASIOBackend::freewheel (bool onoff)
{
	if (onoff == _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;
	// wake up freewheeling thread
	if (0 == pthread_mutex_trylock (&_freewheel_mutex)) {
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}
	return 0;
}

float
ASIOBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
ASIOBackend::raw_buffer_size (DataType t)
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
ASIOBackend::sample_time ()
{
	return _processed_samples;
}

framepos_t
ASIOBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
ASIOBackend::samples_since_cycle_start ()
{
	if (!_active_cb || !_run || _freewheeling || _freewheel) {
		return 0;
	}
	if (_last_process_start == 0) {
		return 0;
	}

#if 0 // TODO
	const uint64_t now = AudioGetCurrentHostTime ();
	const int64_t elapsed_time_ns = AudioConvertHostTimeToNanos(now - _last_process_start);
	return std::max((pframes_t)0, (pframes_t)rint(1e-9 * elapsed_time_ns * _samplerate));
#else
	return 0;
#endif
}

void *
ASIOBackend::asio_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
ASIOBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t thread_id;
	pthread_attr_t attr;
	size_t stacksize = 100000;

	ThreadData* td = new ThreadData (this, func, stacksize);

	if (_realtime_pthread_create (SCHED_FIFO, -21, stacksize,
				&thread_id, asio_process_thread, td)) {
		pthread_attr_init (&attr);
		pthread_attr_setstacksize (&attr, stacksize);
		if (pthread_create (&thread_id, &attr, asio_process_thread, td)) {
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
ASIOBackend::join_process_threads ()
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
ASIOBackend::in_process_thread ()
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
ASIOBackend::process_thread_count ()
{
	return _threads.size ();
}

void
ASIOBackend::update_latencies ()
{
	// trigger latency callback in RT thread (locked graph)
	port_connect_add_remove_callback();
}

/* PORTENGINE API */

void*
ASIOBackend::private_handle () const
{
	return NULL;
}

const std::string&
ASIOBackend::my_name () const
{
	return _instance_name;
}

bool
ASIOBackend::available () const
{
	return _run && _active_fw && _active_cb;
}

uint32_t
ASIOBackend::port_name_size () const
{
	return 256;
}

int
ASIOBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<ASIOBackendPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
ASIOBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<ASIOBackendPort*>(port)->name ();
}

int
ASIOBackend::get_port_property (PortHandle port, const std::string& key, std::string& value, std::string& type) const
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::get_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name") {
		type = "";
		value = static_cast<ASIOBackendPort*>(port)->pretty_name ();
		if (!value.empty()) {
			return 0;
		}
	}
	return -1;
}

PortEngine::PortHandle
ASIOBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle) find_port (name);
	return port;
}

int
ASIOBackend::get_ports (
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
		ASIOBackendPort* port = _ports[i];
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
ASIOBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<ASIOBackendPort*>(port)->type ();
}

PortEngine::PortHandle
ASIOBackend::register_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
ASIOBackend::add_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("ASIOBackend::register_port: Port already exists:")
				<< " (" << name << ")" << endmsg;
		return 0;
	}
	ASIOBackendPort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new ASIOPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new WindowsMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << _("ASIOBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.push_back (port);

	return port;
}

void
ASIOBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!_run) {
		return;
	}
	ASIOBackendPort* port = static_cast<ASIOBackendPort*>(port_handle);
	std::vector<ASIOBackendPort*>::iterator i = std::find (_ports.begin (), _ports.end (), static_cast<ASIOBackendPort*>(port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("ASIOBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all(port_handle);
	_ports.erase (i);
	delete port;
}

int
ASIOBackend::register_system_audio_ports()
{
	LatencyRange lr;

	const uint32_t a_ins = _n_inputs;
	const uint32_t a_out = _n_outputs;

	const uint32_t asio_reported_input_latency =  0; // TODO _pcmio->get_latency(_audio_device, true);
	const uint32_t asio_reported_output_latency = 0; // TODO _pcmio->get_latency(_audio_device, false);

	/* audio ports */
	lr.min = lr.max = asio_reported_input_latency + (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (uint32_t i = 0; i < a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		ASIOBackendPort *cp = static_cast<ASIOBackendPort*>(p);
		//cp->set_pretty_name (_pcmio->cached_port_name(i, true)); // TODO
		_system_inputs.push_back(cp);
	}

	lr.min = lr.max = asio_reported_output_latency + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (uint32_t i = 0; i < a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		ASIOBackendPort *cp = static_cast<ASIOBackendPort*>(p);
		//cp->set_pretty_name (_pcmio->cached_port_name(i, false)); // TODO
		_system_outputs.push_back(cp);
	}
	return 0;
}

void
ASIOBackend::unregister_ports (bool system_only)
{
	size_t i = 0;
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();
	while (i <  _ports.size ()) {
		ASIOBackendPort* port = _ports[i];
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
ASIOBackend::connect (const std::string& src, const std::string& dst)
{
	ASIOBackendPort* src_port = find_port (src);
	ASIOBackendPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("ASIOBackend::connect: Invalid Source port:")
				<< " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("ASIOBackend::connect: Invalid Destination port:")
			<< " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
ASIOBackend::disconnect (const std::string& src, const std::string& dst)
{
	ASIOBackendPort* src_port = find_port (src);
	ASIOBackendPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("ASIOBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
ASIOBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	ASIOBackendPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("ASIOBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("ASIOBackend::connect: Invalid Destination Port")
			<< " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<ASIOBackendPort*>(src)->connect (dst_port);
}

int
ASIOBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	ASIOBackendPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("ASIOBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<ASIOBackendPort*>(src)->disconnect (dst_port);
}

int
ASIOBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<ASIOBackendPort*>(port)->disconnect_all ();
	return 0;
}

bool
ASIOBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<ASIOBackendPort*>(port)->is_connected ();
}

bool
ASIOBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	ASIOBackendPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("ASIOBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<ASIOBackendPort*>(src)->is_connected (dst_port);
}

bool
ASIOBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<ASIOBackendPort*>(port)->is_physically_connected ();
}

int
ASIOBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::vector<ASIOBackendPort*>& connected_ports = static_cast<ASIOBackendPort*>(port)->get_connections ();

	for (std::vector<ASIOBackendPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
ASIOBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t** buf, void* port_buffer,
		uint32_t event_index)
{
	if (!buf || !port_buffer) return -1;
	WindowsMidiBuffer& source = * static_cast<WindowsMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	WindowsMidiEvent * const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size = event->size ();
	*buf = event->data ();
	return 0;
}

int
ASIOBackend::midi_event_put (
		void* port_buffer,
		pframes_t timestamp,
		const uint8_t* buffer, size_t size)
{
	if (!buffer || !port_buffer) return -1;
	WindowsMidiBuffer& dst = * static_cast<WindowsMidiBuffer*>(port_buffer);
	if (dst.size () && (pframes_t)dst.back ()->timestamp () > timestamp) {
#ifndef NDEBUG
		// nevermind, ::get_buffer() sorts events
		fprintf (stderr, "WindowsMidiBuffer: unordered event: %d > %d\n",
				(pframes_t)dst.back ()->timestamp (), timestamp);
#endif
	}
	dst.push_back (boost::shared_ptr<WindowsMidiEvent>(new WindowsMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
ASIOBackend::get_midi_event_count (void* port_buffer)
{
	if (!port_buffer) return 0;
	return static_cast<WindowsMidiBuffer*>(port_buffer)->size ();
}

void
ASIOBackend::midi_clear (void* port_buffer)
{
	if (!port_buffer) return;
	WindowsMidiBuffer * buf = static_cast<WindowsMidiBuffer*>(port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
ASIOBackend::can_monitor_input () const
{
	return false;
}

int
ASIOBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
ASIOBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
ASIOBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
ASIOBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackendPort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<ASIOBackendPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
ASIOBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackendPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}
	ASIOBackendPort* p = static_cast<ASIOBackendPort*>(port);
	assert(p);

	r = p->latency_range (for_playback);
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
ASIOBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("ASIOBackendPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<ASIOBackendPort*>(port)->is_physical ();
}

void
ASIOBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		ASIOBackendPort* port = _ports[i];
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
ASIOBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		ASIOBackendPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
ASIOBackend::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		ASIOBackendPort* port = _ports[i];
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
ASIOBackend::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		ASIOBackendPort* port = _ports[i];
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
ASIOBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	if (!port || !valid_port (port)) return NULL;
	return static_cast<ASIOBackendPort*>(port)->get_buffer (nframes);
}

void
ASIOBackend::pre_process ()
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
ASIOBackend::freewheel_thread ()
{
	_active_fw = true;
	bool first_run = false;
	/* Freewheeling - use for export.   The first call to
	 * engine.process_callback() after engine.freewheel_callback will
	 * if the first export cycle.
	 * For reliable precise export timing, the calls need to be in sync.
	 *
	 * Furthermore we need to make sure the registered process thread
	 * is correct.
	 *
	 * _freewheeling = GUI thread state as set by ::freewheel()
	 * _freewheel = in sync here (export thread)
	 */
	pthread_mutex_lock (&_freewheel_mutex);
	while (_run) {
		// check if we should run,
		if (_freewheeling != _freewheel) {
			if (!_freewheeling) {
				// prepare leaving freewheeling mode
				_freewheel = false; // first mark as disabled
				_reinit_thread_callback = true; // hand over _main_thread
				_freewheel_ack = false; // prepare next handshake
#if 0 // TODO disable hadrware MIDI threads during freewheeling
				_midiio->set_enabled(true);
#endif
			} else {
				first_run = true;
				_freewheel = true;
			}
		}

		if (!_freewheel || !_freewheel_ack) {
			// wait for a change, we use a timed wait to
			// terminate early in case some error sets _run = 0
			struct timeval tv;
			struct timespec ts;
			gettimeofday (&tv, NULL);
			ts.tv_sec = tv.tv_sec + 3;
			ts.tv_nsec = 0;
			pthread_cond_timedwait (&_freewheel_signal, &_freewheel_mutex, &ts);
			continue;
		}

		if (first_run) {
			// tell the engine we're ready to GO.
			engine.freewheel_callback (_freewheeling);
			first_run = false;
			_main_thread = pthread_self();
			AudioEngine::thread_init_callback (this);
#if 0 // TODO
			_midiio->set_enabled(false);
#endif
		}

		// process port updates first in every cycle.
		pre_process();

		// prevent device changes
		pthread_mutex_lock (&_process_callback_mutex);

		/* Freewheelin' */
		
		// clear input buffers
		for (std::vector<ASIOBackendPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
		}
		for (std::vector<ASIOBackendPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
			static_cast<WindowsMidiBuffer*>((*it)->get_buffer(0))->clear ();
		}

		_last_process_start = 0;
		if (engine.process_callback (_samples_per_period)) {
			pthread_mutex_unlock (&_process_callback_mutex);
			break;
		}

		pthread_mutex_unlock (&_process_callback_mutex);
		_dsp_load = 1.0;
		Glib::usleep (100); // don't hog cpu
	}

	pthread_mutex_unlock (&_freewheel_mutex);

	_active_fw = false;

	if (_run) {
		// engine.process_callback() returner error
		engine.halted_callback("ASIO Freehweeling aborted.");
	}
	return 0;
}

int
ASIOBackend::process_callback (const uint32_t n_samples, const uint64_t host_time)
{
	uint32_t i = 0;
	uint64_t clock1, clock2;

	_active_cb = true;

	if (_run && _freewheel && !_freewheel_ack) {
		// acknowledge freewheeling; hand-over thread ID
		pthread_mutex_lock (&_freewheel_mutex);
		if (_freewheel) _freewheel_ack = true;
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}

	if (!_run || _freewheel || _preinit) {
		// NB if we return 1, the output is
		// zeroed by the asio callback
		return 1;
	}

	if (_reinit_thread_callback || _main_thread != pthread_self()) {
		_reinit_thread_callback = false;
		_main_thread = pthread_self();
		AudioEngine::thread_init_callback (this);
	}

	if (pthread_mutex_trylock (&_process_callback_mutex)) {
		// block while devices are added/removed
#ifndef NDEBUG
		printf("Xrun due to device change\n");
#endif
		engine.Xrun();
		return 1;
	}
	/* port-connection change */
	pre_process();

	// cycle-length in usec
	const double nominal_time = 1e6 * n_samples / _samplerate;

	clock1 = g_get_monotonic_time();

	/* get midi */
	i=0;
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
		WindowsMidiBuffer* mbuf = static_cast<WindowsMidiBuffer*>((*it)->get_buffer(0));
		mbuf->clear();
#if 0 // TODO
		uint64_t time_ns;
		uint8_t data[128];
		size_t size = sizeof(data);
		while (_midiio->recv_event (i, nominal_time, time_ns, data, size)) {
			pframes_t time = floor((float) time_ns * _samplerate * 1e-9);
			assert (time < n_samples);
			midi_event_put((void*)mbuf, time, data, size);
			size = sizeof(data);
		}
#endif
	}

	/* get audio */
	i = 0;
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++i) {
#if 0 // TODO
		_pcmio->get_capture_channel (i, (float*)((*it)->get_buffer(n_samples)), n_samples);
#else
		memset ((*it)->get_buffer (n_samples), 0, n_samples * sizeof (Sample));
#endif
	}

	/* clear output buffers */
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		memset ((*it)->get_buffer (n_samples), 0, n_samples * sizeof (Sample));
	}

#if 0 // TODO; sync Audio + midi
	_midiio->start_cycle();
#endif
	_last_process_start = host_time;

	if (engine.process_callback (n_samples)) {
		fprintf(stderr, "ENGINE PROCESS ERROR\n");
		_active_cb = false;
		pthread_mutex_unlock (&_process_callback_mutex);
		return -1;
	}

	/* mixdown midi */
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
		static_cast<WindowsMidiPort*>(*it)->get_buffer(0);
	}

	/* queue outgoing midi */
#if 0 // TODO
	i = 0;
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
#if 0 // something's still b0rked with WindowsMidiIo::send_events()
		const WindowsMidiBuffer *src = static_cast<const WindowsMidiPort*>(*it)->const_buffer();
		_midiio->send_events (i, nominal_time, (void*)src);
#else // works..
		const WindowsMidiBuffer *src = static_cast<const WindowsMidiPort*>(*it)->const_buffer();
		for (WindowsMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
			_midiio->send_event (i, (*mit)->timestamp() / nominal_time, (*mit)->data(), (*mit)->size());
		}
#endif
	}
#endif

	/* write back audio */
#if 0 // TODO
	i = 0;
	for (std::vector<ASIOBackendPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
		_pcmio->set_playback_channel (i, (float const*)(*it)->get_buffer (n_samples), n_samples);
	}
#endif

	_processed_samples += n_samples;

	/* calc DSP load. */
	clock2 = g_get_monotonic_time();
	const int64_t elapsed_time = clock2 - clock1;
	_dsp_load = elapsed_time / nominal_time;

	pthread_mutex_unlock (&_process_callback_mutex);
	return 0;
}

void
ASIOBackend::error_callback ()
{
#if 0 // TODO
	_pcmio->set_error_callback (NULL, NULL);
	_pcmio->set_sample_rate_callback (NULL, NULL);
	_pcmio->set_xrun_callback (NULL, NULL);
	_midiio->set_port_changed_callback(NULL, NULL);
#endif
	engine.halted_callback("ASIO Process aborted.");
	_active_cb = false;
}

void
ASIOBackend::xrun_callback ()
{
	engine.Xrun ();
}

void
ASIOBackend::buffer_size_callback ()
{
#if 0 // TODO
	uint32_t bs = _pcmio->samples_per_period();
	if (bs == _samples_per_period) {
		return;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (_samples_per_period);
#endif
}

void
ASIOBackend::sample_rate_callback ()
{
#if 0  // TODO
	_pcmio->set_error_callback (NULL, NULL);
	_pcmio->set_sample_rate_callback (NULL, NULL);
	_pcmio->set_xrun_callback (NULL, NULL);
	_midiio->set_port_changed_callback(NULL, NULL);
	engine.halted_callback("Sample Rate Changed.");
	stop();
#endif
}

void
ASIOBackend::hw_changed_callback ()
{
#if 0 // TODO
	_reinit_thread_callback = true;
	engine.request_device_list_update();
#endif
}

/******************************************************************************/

static boost::shared_ptr<ASIOBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	"ASIO",
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
		_instance.reset (new ASIOBackend (e, _descriptor));
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
ASIOBackendPort::ASIOBackendPort (ASIOBackend &b, const std::string& name, PortFlags flags)
	: _osx_backend (b)
	, _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

ASIOBackendPort::~ASIOBackendPort () {
	disconnect_all ();
}


int ASIOBackendPort::connect (ASIOBackendPort *port)
{
	if (!port) {
		PBD::error << _("ASIOBackendPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("ASIOBackendPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("ASIOBackendPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("ASIOBackendPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("ASIOBackendPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("ASIOBackendPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}


void ASIOBackendPort::_connect (ASIOBackendPort *port, bool callback)
{
	_connections.push_back (port);
	if (callback) {
		port->_connect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), true);
	}
}

int ASIOBackendPort::disconnect (ASIOBackendPort *port)
{
	if (!port) {
		PBD::error << _("ASIOBackendPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("ASIOBackendPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void ASIOBackendPort::_disconnect (ASIOBackendPort *port, bool callback)
{
	std::vector<ASIOBackendPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);

	assert (it != _connections.end ());

	_connections.erase (it);

	if (callback) {
		port->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void ASIOBackendPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_osx_backend.port_connect_callback (name(),  _connections.back ()->name(), false);
		_connections.pop_back ();
	}
}

bool
ASIOBackendPort::is_connected (const ASIOBackendPort *port) const
{
	return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
}

bool ASIOBackendPort::is_physically_connected () const
{
	for (std::vector<ASIOBackendPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

/******************************************************************************/

ASIOPort::ASIOPort (ASIOBackend &b, const std::string& name, PortFlags flags)
	: ASIOBackendPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock(_buffer, sizeof (_buffer));
}

ASIOPort::~ASIOPort () { }

void* ASIOPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<ASIOBackendPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			ASIOPort const * source = static_cast<const ASIOPort*>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<const ASIOPort*>(*it);
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


WindowsMidiPort::WindowsMidiPort (ASIOBackend &b, const std::string& name, PortFlags flags)
	: ASIOBackendPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();
}

WindowsMidiPort::~WindowsMidiPort () { }

struct MidiEventSorter {
	bool operator() (const boost::shared_ptr<WindowsMidiEvent>& a, const boost::shared_ptr<WindowsMidiEvent>& b) {
		return *a < *b;
	}
};

void* WindowsMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		for (std::vector<ASIOBackendPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const WindowsMidiBuffer * src = static_cast<const WindowsMidiPort*>(*i)->const_buffer ();
			for (WindowsMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (boost::shared_ptr<WindowsMidiEvent>(new WindowsMidiEvent (**it)));
			}
		}
		std::sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}
	return &(_buffer[_bufperiod]);
}

WindowsMidiEvent::WindowsMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
	, _data (0)
{
	if (size > 0) {
		_data = (uint8_t*) malloc (size);
		memcpy (_data, data, size);
	}
}

WindowsMidiEvent::WindowsMidiEvent (const WindowsMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
	, _data (0)
{
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

WindowsMidiEvent::~WindowsMidiEvent () {
	free (_data);
};
