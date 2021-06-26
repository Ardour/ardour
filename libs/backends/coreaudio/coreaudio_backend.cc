/*
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#include <regex.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <mach/thread_policy.h>
#include <mach/thread_act.h>

#undef nil

#include <glibmm.h>

#include "coreaudio_backend.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "pbd/i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t CoreAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> CoreAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> CoreAudioBackend::_duplex_audio_device_status;
std::vector<AudioBackend::DeviceStatus> CoreAudioBackend::_input_audio_device_status;
std::vector<AudioBackend::DeviceStatus> CoreAudioBackend::_output_audio_device_status;


/* static class instance access */
static void hw_changed_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->hw_changed_callback();
}

static void error_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->error_callback();
}

static void xrun_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->xrun_callback();
}

static void buffer_size_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->buffer_size_callback();
}

static void sample_rate_callback_ptr (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	d->sample_rate_callback();
}

static void midi_port_change (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend *>(arg);
	d->coremidi_rediscover ();
}


CoreAudioBackend::CoreAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, PortEngineSharedImpl (e, s_instance_name)
	, _run (false)
	, _active_ca (false)
	, _active_fw (false)
	, _freewheeling (false)
	, _freewheel (false)
	, _freewheel_ack (false)
	, _reinit_thread_callback (false)
	, _measure_latency (false)
	, _last_process_start (0)
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
{
	_instance_name = s_instance_name;
	pthread_mutex_init (&_process_callback_mutex, 0);
	pthread_mutex_init (&_freewheel_mutex, 0);
	pthread_cond_init  (&_freewheel_signal, 0);

	_port_connection_queue.reserve (128);

	_pcmio = new CoreAudioPCM ();
	_midiio = new CoreMidiIo ();

	_pcmio->set_hw_changed_callback (hw_changed_callback_ptr, this);
	_pcmio->discover();
}

CoreAudioBackend::~CoreAudioBackend ()
{
	delete _pcmio; _pcmio = 0;
	delete _midiio; _midiio = 0;

	clear_ports ();

	pthread_mutex_destroy (&_process_callback_mutex);
	pthread_mutex_destroy (&_freewheel_mutex);
	pthread_cond_destroy  (&_freewheel_signal);
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
	_duplex_audio_device_status.clear();
	std::map<size_t, std::string> devices;
	_pcmio->duplex_device_list(devices);

	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_input_audio_device == "") _input_audio_device = i->second;
		if (_output_audio_device == "") _output_audio_device = i->second;
		_duplex_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _duplex_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
CoreAudioBackend::enumerate_input_devices () const
{
	_input_audio_device_status.clear();
	std::map<size_t, std::string> devices;
	_pcmio->input_device_list(devices);

	_input_audio_device_status.push_back (DeviceStatus (get_standard_device_name(DeviceNone), true));
	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_input_audio_device == "") _input_audio_device = i->second;
		_input_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _input_audio_device_status;
}


std::vector<AudioBackend::DeviceStatus>
CoreAudioBackend::enumerate_output_devices () const
{
	_output_audio_device_status.clear();
	std::map<size_t, std::string> devices;
	_pcmio->output_device_list(devices);

	_output_audio_device_status.push_back (DeviceStatus (get_standard_device_name(DeviceNone), true));
	for (std::map<size_t, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_output_audio_device == "") _output_audio_device = i->second;
		_output_audio_device_status.push_back (DeviceStatus (i->second, true));
	}
	return _output_audio_device_status;
}

std::vector<float>
CoreAudioBackend::available_sample_rates (const std::string& device) const
{
	std::vector<float> sr;
	_pcmio->available_sample_rates (name_to_id (device), sr);
	return sr;
}

std::vector<float>
CoreAudioBackend::available_sample_rates2 (const std::string& input_device, const std::string& output_device) const
{
	std::vector<float> sr;
	std::vector<float> sr_in;
	std::vector<float> sr_out;

	const uint32_t inp = name_to_id (input_device, Input);
	const uint32_t out = name_to_id (output_device, Output);

	if (inp == UINT32_MAX && out == UINT32_MAX) {
		return sr;
	} else if (inp == UINT32_MAX) {
		_pcmio->available_sample_rates (out, sr_out);
		return sr_out;
	} else if (out == UINT32_MAX) {
		_pcmio->available_sample_rates (inp, sr_in);
		return sr_in;
	} else {
		_pcmio->available_sample_rates (inp, sr_in);
		_pcmio->available_sample_rates (out, sr_out);
		// TODO allow to use different SR per device, tweak aggregate
		std::set_intersection (sr_in.begin(), sr_in.end(), sr_out.begin(), sr_out.end(), std::back_inserter(sr));
		return sr;
	}
}

std::vector<uint32_t>
CoreAudioBackend::available_buffer_sizes (const std::string& device) const
{
	std::vector<uint32_t> bs;
	_pcmio->available_buffer_sizes (name_to_id (device), bs);
	return bs;
}

std::vector<uint32_t>
CoreAudioBackend::available_buffer_sizes2 (const std::string& input_device, const std::string& output_device) const
{
	std::vector<uint32_t> bs;
	std::vector<uint32_t> bs_in;
	std::vector<uint32_t> bs_out;
	const uint32_t inp = name_to_id (input_device, Input);
	const uint32_t out = name_to_id (output_device, Output);
	if (inp == UINT32_MAX && out == UINT32_MAX) {
		return bs;
	} else if (inp == UINT32_MAX) {
		_pcmio->available_buffer_sizes (out, bs_out);
		return bs_out;
	} else if (out == UINT32_MAX) {
		_pcmio->available_buffer_sizes (inp, bs_in);
		return bs_in;
	} else {
		_pcmio->available_buffer_sizes (inp, bs_in);
		_pcmio->available_buffer_sizes (out, bs_out);
		std::set_intersection (bs_in.begin(), bs_in.end(), bs_out.begin(), bs_out.end(), std::back_inserter(bs));
		return bs;
	}
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
	return true;
}

int
CoreAudioBackend::set_device_name (const std::string& d)
{
	int rv = 0;
	rv |= set_input_device_name (d);
	rv |= set_output_device_name (d);
	return rv;
}

int
CoreAudioBackend::set_input_device_name (const std::string& d)
{
	_input_audio_device = d;
	const float sr = _pcmio->current_sample_rate(name_to_id(_input_audio_device, Input));
	if (sr > 0) { set_sample_rate(sr); }
	return 0;
}

int
CoreAudioBackend::set_output_device_name (const std::string& d)
{
	_output_audio_device = d;
	// TODO check SR.
	const float sr = _pcmio->current_sample_rate(name_to_id(_output_audio_device, Output));
	if (sr > 0) { set_sample_rate(sr); }
	return 0;
}

int
CoreAudioBackend::set_sample_rate (float sr)
{
	std::vector<float> srs = available_sample_rates2 (_input_audio_device, _output_audio_device);
	if (std::find(srs.begin(), srs.end(), sr) == srs.end()) {
		return -1;
	}
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
	if (!_run) {
		_samples_per_period = bs;
		engine.buffer_size_change (bs);
	}
	_pcmio->set_samples_per_period(bs);
	if (_run) {
		pbd_mach_set_realtime_policy (_main_thread, 1e9 * bs / _samplerate, true);
	}
	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i) {
		pbd_mach_set_realtime_policy (*i, 1e9 * bs / _samplerate, false);
	}
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

/* Retrieving parameters */
std::string
CoreAudioBackend::device_name () const
{
	return "";
}

std::string
CoreAudioBackend::input_device_name () const
{
	return _input_audio_device;
}

std::string
CoreAudioBackend::output_device_name () const
{
	return _output_audio_device;
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
CoreAudioBackend::systemic_hw_input_latency () const
{
	if (name_to_id (_input_audio_device) != UINT32_MAX) {
		return _pcmio->get_latency(name_to_id(_input_audio_device, Input), true);
	}
	return 0;
}

uint32_t
CoreAudioBackend::systemic_hw_output_latency () const
{
	if (name_to_id (_output_audio_device) != UINT32_MAX) {
		return _pcmio->get_latency(name_to_id(_output_audio_device, Output), false);
	}
	return 0;
}

/* MIDI */

std::vector<std::string>
CoreAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		_midi_options.push_back (_("CoreMidi"));
		_midi_options.push_back (get_standard_device_name(DeviceNone));
	}
	return _midi_options;
}

int
CoreAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != get_standard_device_name(DeviceNone) && opt != _("CoreMidi")) {
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

void
CoreAudioBackend::launch_control_app ()
{
	if (name_to_id (_input_audio_device) != UINT32_MAX) {
		_pcmio->launch_control_app(name_to_id(_input_audio_device, Input));
	}
	if (name_to_id (_output_audio_device) != UINT32_MAX) {
		_pcmio->launch_control_app(name_to_id(_output_audio_device, Output));
	}
}

/* State Control */

static void * pthread_freewheel (void *arg)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend *>(arg);
	pthread_set_name ("CAFreewheel");
	d->freewheel_thread ();
	pthread_exit (0);
	return 0;
}

static int process_callback_ptr (void *arg, const uint32_t n_samples, const uint64_t host_time)
{
	CoreAudioBackend *d = static_cast<CoreAudioBackend*> (arg);
	return d->process_callback(n_samples, host_time);
}

int
CoreAudioBackend::_start (bool for_latency_measurement)
{
	AudioBackend::ErrorCode error_code = NoError;

	if ((!_active_ca || !_active_fw)  && _run) {
		// recover from 'halted', reap threads
		stop();
	}

	if (_active_ca || _active_fw || _run) {
		PBD::error << _("CoreAudioBackend: already active.") << endmsg;
		return BackendReinitializationError;
	}

	clear_ports ();

	uint32_t device1 = name_to_id(_input_audio_device, Input);
	uint32_t device2 = name_to_id(_output_audio_device, Output);

	assert(_active_ca == false);
	assert(_active_fw == false);

	_freewheel_ack = false;
	_reinit_thread_callback = true;
	_last_process_start = 0;

	_pcmio->set_error_callback (error_callback_ptr, this);
	_pcmio->set_buffer_size_callback (buffer_size_callback_ptr, this);
	_pcmio->set_sample_rate_callback (sample_rate_callback_ptr, this);

	_pcmio->pcm_start (device1, device2, _samplerate, _samples_per_period, process_callback_ptr, this, dsp_stats[AudioBackend::DeviceWait]);
#ifndef NDEBUG
	printf("STATE: %d\n", _pcmio->state ());
#endif
	switch (_pcmio->state ()) {
	case 0: /* OK */
		break;
	case -1:
		PBD::error << _("CoreAudioBackend: Invalid Device ID.") << endmsg;
		error_code = AudioDeviceInvalidError;
		break;
	case -2:
		PBD::error << _("CoreAudioBackend: Failed to resolve Device-Component by ID.") << endmsg;
		error_code = AudioDeviceNotAvailableError;
		break;
	case -3:
		PBD::error << _("CoreAudioBackend: failed to open device.") << endmsg;
		error_code = AudioDeviceOpenError;
		break;
	case -4:
		PBD::error << _("CoreAudioBackend: cannot set requested sample rate.") << endmsg;
		error_code = SampleRateNotSupportedError;
		break;
	case -5:
		PBD::error << _("CoreAudioBackend: cannot configure requested buffer size.") << endmsg;
		error_code = PeriodSizeNotSupportedError;
		break;
	case -6:
		PBD::error << _("CoreAudioBackend: unsupported sample format.") << endmsg;
		error_code = SampleFormatNotSupportedError;
		break;
	case -7:
		PBD::error << _("CoreAudioBackend: Failed to enable Device.") << endmsg;
		error_code = BackendInitializationError; // XXX
		break;
	case -8:
		PBD::error << _("CoreAudioBackend: Cannot allocate buffers, out-of-memory.") << endmsg;
		error_code = OutOfMemoryError;
		break;
	case -9:
		PBD::error << _("CoreAudioBackend: Failed to set device-property listeners.") << endmsg;
		error_code = BackendInitializationError; // XXX
		break;
	case -10:
		PBD::error << _("CoreAudioBackend: Setting Process Callback failed.") << endmsg;
		error_code = AudioDeviceIOError;
		break;
	case -11:
		PBD::error << _("CoreAudioBackend: cannot use requested period size.") << endmsg;
		error_code = PeriodSizeNotSupportedError;
		break;
	case -12:
		PBD::error << _("CoreAudioBackend: cannot create aggregate device.") << endmsg;
		error_code = DeviceConfigurationNotSupportedError;
		break;
	default:
		PBD::error << _("CoreAudioBackend: initialization failure.") << endmsg;
		error_code = BackendInitializationError;
		break;
	}
	if (_pcmio->state ()) {
		return error_code;
	}

	if (_n_outputs != _pcmio->n_playback_channels ()) {
		if (_n_outputs == 0) {
			_n_outputs = _pcmio->n_playback_channels ();
		} else {
			_n_outputs = std::min (_n_outputs, _pcmio->n_playback_channels ());
		}
		PBD::info << _("CoreAudioBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmio->n_capture_channels ()) {
		if (_n_inputs == 0) {
			_n_inputs = _pcmio->n_capture_channels ();
		} else {
			_n_inputs = std::min (_n_inputs, _pcmio->n_capture_channels ());
		}
		PBD::info << _("CoreAudioBackend: adjusted input channel count to match device.") << endmsg;
	}

	if (_pcmio->sample_rate() != _samplerate) {
		_samplerate = _pcmio->sample_rate();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("CoreAudioBackend: sample rate does not match.") << endmsg;
	}

	_measure_latency = for_latency_measurement;

	_preinit = true;
	_run = true;
	g_atomic_int_set (&_port_change_flag, 0);

	if (_midi_driver_option == _("CoreMidi")) {
		_midiio->set_enabled(true);
		_midiio->set_port_changed_callback(midi_port_change, this);
		_midiio->start(); // triggers port discovery, callback coremidi_rediscover()
	}

	if (register_system_audio_ports()) {
		PBD::error << _("CoreAudioBackend: failed to register system ports.") << endmsg;
		_run = false;
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("CoreAudioBackend: Could not re-establish ports.") << endmsg;
		_run = false;
		return PortReconnectError;
	}

	if (pthread_create (&_freeewheel_thread, NULL, pthread_freewheel, this))
	{
		PBD::error << _("CoreAudioBackend: failed to create process thread.") << endmsg;
		delete _pcmio; _pcmio = 0;
		_run = false;
		return ProcessThreadStartError;
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
		return FreewheelThreadStartError;
	}

	if (!_active_ca) {
		PBD::error << _("CoreAudioBackend: failed to start coreaudio.") << endmsg;
		stop();
		_run = false;
		return ProcessThreadStartError;
	}

	engine.reconnect_ports ();

	// force  an initial registration_callback() & latency re-compute
	g_atomic_int_set (&_port_change_flag, 1);
	pre_process ();

	_dsp_load_calc.reset ();
	// all systems go.
	_pcmio->set_xrun_callback (xrun_callback_ptr, this);
	_preinit = false;

	return NoError;
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
	_midiio->stop();

	pthread_mutex_lock (&_freewheel_mutex);
	pthread_cond_signal (&_freewheel_signal);
	pthread_mutex_unlock (&_freewheel_mutex);

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
	// wake up freewheeling thread
	if (0 == pthread_mutex_trylock (&_freewheel_mutex)) {
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}
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
samplepos_t
CoreAudioBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
CoreAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
CoreAudioBackend::samples_since_cycle_start ()
{
	if (!_active_ca || !_run || _freewheeling || _freewheel) {
		return 0;
	}
	if (_last_process_start == 0) {
		return 0;
	}

	const uint64_t now = AudioGetCurrentHostTime ();
	const int64_t elapsed_time_ns = AudioConvertHostTimeToNanos(now - _last_process_start);
	return std::max((pframes_t)0, (pframes_t)rint(1e-9 * elapsed_time_ns * _samplerate));
}

uint32_t
CoreAudioBackend::name_to_id(std::string device_name, DeviceFilter filter) const {
	uint32_t device_id = UINT32_MAX;
	std::map<size_t, std::string> devices;
	switch (filter) {
		case Input:
			_pcmio->input_device_list (devices);
			break;
		case Output:
			_pcmio->output_device_list (devices);
			break;
		case Duplex:
			_pcmio->duplex_device_list (devices);
			break;
		case All:
		default:
			_pcmio->device_list (devices);
			break;
	}

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
	pthread_t   thread_id;
	ThreadData* td = new ThreadData (this, func, PBD_RT_STACKSIZE_PROC);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_PROC, PBD_RT_STACKSIZE_PROC,
	                              &thread_id, coreaudio_process_thread, td)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &thread_id, coreaudio_process_thread, td)) {
			PBD::error << _("AudioEngine: cannot create process thread.") << endmsg;
			return -1;
		}
		PBD::warning << _("AudioEngine: process thread failed to acquire realtime permissions.") << endmsg;
	}

	if (pbd_mach_set_realtime_policy (thread_id, 1e9 * _samples_per_period / _samplerate, false)) {
		PBD::warning << _("AudioEngine: process thread failed to set mach realtime policy.") << endmsg;
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

int
CoreAudioBackend::register_system_audio_ports()
{
	LatencyRange lr;

	const uint32_t a_ins = _n_inputs;
	const uint32_t a_out = _n_outputs;

	const uint32_t coreaudio_reported_input_latency = _pcmio->get_latency(name_to_id(_input_audio_device, Input), true);
	const uint32_t coreaudio_reported_output_latency = _pcmio->get_latency(name_to_id(_output_audio_device, Output), false);

#ifndef NDEBUG
	printf("COREAUDIO LATENCY: i:%d, o:%d\n",
	       coreaudio_reported_input_latency,
	       coreaudio_reported_output_latency);
#endif

	/* audio ports */
	lr.min = lr.max = _measure_latency ? 0 : _systemic_audio_input_latency;
	for (uint32_t i = 0; i < a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i+1);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		BackendPortPtr cp = boost::dynamic_pointer_cast<BackendPort>(p);
		cp->set_hw_port_name (_pcmio->cached_port_name(i, true));
		_system_inputs.push_back(cp);
	}

	lr.min = lr.max = _measure_latency ? 0 : _systemic_audio_output_latency;
	for (uint32_t i = 0; i < a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i+1);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		BackendPortPtr cp = boost::dynamic_pointer_cast<BackendPort>(p);
		cp->set_hw_port_name (_pcmio->cached_port_name(i, false));
		_system_outputs.push_back(cp);
	}
	return 0;
}

void
CoreAudioBackend::coremidi_rediscover()
{
	if (!_run) { return; }
	assert(_midi_driver_option == _("CoreMidi"));

	pthread_mutex_lock (&_process_callback_mutex);

	for (std::vector<BackendPortPtr>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end ();) {
		bool found = false;
		for (size_t i = 0; i < _midiio->n_midi_outputs(); ++i) {
			if ((*it)->name() == _midiio->port_id(i, false)) {
				found = true;
				break;
			}
		}
		if (found) {
			++it;
		} else {
#ifndef NDEBUG
			printf("unregister MIDI Output: %s\n", (*it)->name().c_str());
#endif
			g_atomic_int_set (&_port_change_flag, 1);
			unregister_port((*it));
			it = _system_midi_out.erase(it);
		}
	}

	for (std::vector<BackendPortPtr>::iterator it = _system_midi_in.begin (); it != _system_midi_in.end ();) {
		bool found = false;
		for (size_t i = 0; i < _midiio->n_midi_inputs(); ++i) {
			if ((*it)->name() == _midiio->port_id(i, true)) {
				found = true;
				break;
			}
		}
		if (found) {
			++it;
		} else {
#ifndef NDEBUG
			printf("unregister MIDI Input: %s\n", (*it)->name().c_str());
#endif
			g_atomic_int_set (&_port_change_flag, 1);
			unregister_port((*it));
			it = _system_midi_in.erase(it);
		}
	}

	for (size_t i = 0; i < _midiio->n_midi_inputs(); ++i) {
		std::string name = _midiio->port_id(i, true);
		if (find_port_in(_system_midi_in, name)) {
			continue;
		}

#ifndef NDEBUG
		printf("register MIDI Input: %s\n", name.c_str());
#endif
		PortPtr p = add_port(name, DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) {
			fprintf(stderr, "failed to register MIDI IN: %s\n", name.c_str());
			continue;
		}
		LatencyRange lr;
		lr.min = lr.max = _samples_per_period; // TODO add per-port midi-systemic latency
		set_latency_range (p, false, lr);
		BackendPortPtr pp = boost::dynamic_pointer_cast<BackendPort>(p);
		pp->set_hw_port_name(_midiio->port_name(i, true));
		_system_midi_in.push_back(pp);
		g_atomic_int_set (&_port_change_flag, 1);
	}

	for (size_t i = 0; i < _midiio->n_midi_outputs(); ++i) {
		std::string name = _midiio->port_id(i, false);
		if (find_port_in(_system_midi_out, name)) {
			continue;
		}

#ifndef NDEBUG
		printf("register MIDI OUT: %s\n", name.c_str());
#endif
		PortPtr p = add_port(name, DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) {
			fprintf(stderr, "failed to register MIDI OUT: %s\n", name.c_str());
			continue;
		}
		LatencyRange lr;
		lr.min = lr.max = _samples_per_period; // TODO add per-port midi-systemic latency
		set_latency_range (p, false, lr);
		BackendPortPtr pp = boost::dynamic_pointer_cast<BackendPort>(p);
		pp->set_hw_port_name(_midiio->port_name(i, false));
		_system_midi_out.push_back(pp);
		g_atomic_int_set (&_port_change_flag, 1);
	}


	assert(_system_midi_out.size() == _midiio->n_midi_outputs());
	assert(_system_midi_in.size() == _midiio->n_midi_inputs());

	pthread_mutex_unlock (&_process_callback_mutex);
}

BackendPort*
CoreAudioBackend::port_factory (std::string const & name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	BackendPort* port = 0;

	switch (type) {
		case DataType::AUDIO:
			port = new CoreAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new CoreMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << string_compose (_("%1::register_port: Invalid Data Type."), _instance_name) << endmsg;
			return 0;
	}

	return port;
}

/* MIDI */
int
CoreAudioBackend::midi_event_get (
	pframes_t& timestamp,
	size_t& size, uint8_t const** buf, void* port_buffer,
	uint32_t event_index)
{
	if (!buf || !port_buffer) return -1;
	CoreMidiBuffer& source = * static_cast<CoreMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	CoreMidiEvent const& event = source[event_index];

	timestamp = event.timestamp ();
	size = event.size ();
	*buf = event.data ();
	return 0;
}

int
CoreAudioBackend::_midi_event_put (
	void* port_buffer,
	pframes_t timestamp,
	const uint8_t* buffer, size_t size)
{
	if (!buffer || !port_buffer) return -1;
	if (size >= MaxCoreMidiEventSize) {
		return -1;
	}
	CoreMidiBuffer& dst = * static_cast<CoreMidiBuffer*>(port_buffer);
#ifndef NDEBUG
	if (dst.size () && (pframes_t)dst.back ().timestamp () > timestamp) {
		// nevermind, ::get_buffer() sorts events
		fprintf (stderr, "CoreMidiBuffer: unordered event: %d > %d\n",
		         (pframes_t)dst.back ().timestamp (), timestamp);
	}
#endif
	dst.push_back (CoreMidiEvent (timestamp, buffer, size));
	return 0;
}


uint32_t
CoreAudioBackend::get_midi_event_count (void* port_buffer)
{
	if (!port_buffer) return 0;
	return static_cast<CoreMidiBuffer*>(port_buffer)->size ();
}

void
CoreAudioBackend::midi_clear (void* port_buffer)
{
	if (!port_buffer) return;
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
CoreAudioBackend::set_latency_range (PortEngine::PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	if (!valid_port (port)) {
		PBD::warning << _("BackendPort::set_latency_range (): invalid port.") << endmsg;
		return;
	}
	port->set_latency_range (latency_range, for_playback);
}

LatencyRange
CoreAudioBackend::get_latency_range (PortEngine::PortHandle port_handle, bool for_playback)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::warning << _("BackendPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}

	r = port->latency_range (for_playback);
	if (port->is_physical() && port->is_terminal() && port->type() == DataType::AUDIO) {
		if (port->is_input() && for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
		if (port->is_output() && !for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
	}
	return r;
}

/* Getting access to the data buffer for a port */

void*
CoreAudioBackend::get_buffer (PortEngine::PortHandle port_handle, pframes_t nframes)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	assert (port);
	assert (valid_port (port));
	if (!port || !valid_port (port)) return NULL; // XXX remove me
	return port->get_buffer (nframes);
}

void
CoreAudioBackend::pre_process ()
{
	bool connections_changed = false;
	bool ports_changed = false;
	if (!pthread_mutex_trylock (&_port_callback_mutex)) {
		if (g_atomic_int_compare_and_exchange (&_port_change_flag, 1, 0)) {
			ports_changed = true;
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
		update_system_port_latencies ();
		engine.latency_callback(false);
		engine.latency_callback(true);
	}
}

void
CoreAudioBackend::reset_midi_parsers ()
{
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
		boost::shared_ptr<CoreMidiPort> port = boost::dynamic_pointer_cast<CoreMidiPort>(*it);
		if (port) {
			port->reset_parser ();
		}
	}
}

void *
CoreAudioBackend::freewheel_thread ()
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
				reset_midi_parsers ();
				_midiio->set_enabled(true);
				engine.freewheel_callback (_freewheeling);
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
			_midiio->set_enabled(false);
			reset_midi_parsers ();
			pbd_mach_set_realtime_policy (_main_thread, 1e9 * _samples_per_period / _samplerate, true);
		}

		// process port updates first in every cycle.
		pre_process();

		// prevent coreaudio device changes
		pthread_mutex_lock (&_process_callback_mutex);

		/* Freewheelin' */

		// clear input buffers
		for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
		}
		for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
			static_cast<CoreMidiBuffer*>((*it)->get_buffer(0))->clear ();
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
		engine.halted_callback("CoreAudio Freehweeling aborted.");
	}
	return 0;
}
int
CoreAudioBackend::process_callback (const uint32_t n_samples, const uint64_t host_time)
{
	uint32_t i = 0;
	uint64_t clock1;
	PBD::TimerRAII tr (dsp_stats[RunLoop]);

	_active_ca = true;

	if (_run && _freewheel && !_freewheel_ack) {
		// acknowledge freewheeling; hand-over thread ID
		pthread_mutex_lock (&_freewheel_mutex);
		if (_freewheel) _freewheel_ack = true;
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}

	if (!_run || _freewheel || _preinit) {
		// NB if we return 1, the output is
		// zeroed by the coreaudio callback
		_dsp_load_calc.reset ();
		return 1;
	}

	if (_reinit_thread_callback || _main_thread != pthread_self()) {
		_reinit_thread_callback = false;
		_main_thread = pthread_self();
		AudioEngine::thread_init_callback (this);
		pbd_mach_set_realtime_policy (_main_thread, 1e9 * _samples_per_period / _samplerate, true);
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
		boost::shared_ptr<CoreMidiPort> port = boost::dynamic_pointer_cast<CoreMidiPort> (*it);
		if (!port) {
			continue;
		}
		uint64_t time_ns;
		uint8_t data[MaxCoreMidiEventSize];
		size_t size = sizeof(data);

		port->clear_events ();

		while (_midiio->recv_event (i, nominal_time, time_ns, data, size)) {
			pframes_t time = floor((float) time_ns * _samplerate * 1e-9);
			assert (time < n_samples);
			port->parse_events (time, data, size);
			size = sizeof(data); /* prepare for next call to recv_event */
		}
	}

	/* get audio */
	i = 0;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++i) {
		_pcmio->get_capture_channel (i, (float*)(*it)->get_buffer(n_samples), n_samples);
	}

	/* clear output buffers */
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		memset ((*it)->get_buffer (n_samples), 0, n_samples * sizeof (Sample));
	}

	_midiio->start_cycle();
	_last_process_start = host_time;

	if (engine.process_callback (n_samples)) {
		fprintf(stderr, "ENGINE PROCESS ERROR\n");
		//_pcmio->pcm_stop ();
		_active_ca = false;
		pthread_mutex_unlock (&_process_callback_mutex);
		return -1;
	}

	/* mixdown midi */
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
		(*it)->get_buffer(0);
	}

	/* queue outgoing midi */
	i = 0;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
		const CoreMidiBuffer *src = boost::dynamic_pointer_cast<CoreMidiPort>(*it)->const_buffer();
		for (CoreMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
			_midiio->send_event (i, mit->timestamp (), mit->data (), mit->size ());
		}
	}

	/* write back audio */
	i = 0;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
		_pcmio->set_playback_channel (i, (float const*)(*it)->get_buffer (n_samples), n_samples);
	}

	_processed_samples += n_samples;

	/* calc DSP load. */
	_dsp_load_calc.set_max_time (_samplerate, _samples_per_period);
	_dsp_load_calc.set_start_timestamp_us (clock1);
	_dsp_load_calc.set_stop_timestamp_us (g_get_monotonic_time());
	_dsp_load = _dsp_load_calc.get_dsp_load ();

	pthread_mutex_unlock (&_process_callback_mutex);
	return 0;
}

void
CoreAudioBackend::error_callback ()
{
	_pcmio->set_error_callback (NULL, NULL);
	_pcmio->set_sample_rate_callback (NULL, NULL);
	_pcmio->set_xrun_callback (NULL, NULL);
	_midiio->set_port_changed_callback(NULL, NULL);
	engine.halted_callback("CoreAudio Process aborted.");
	_active_ca = false;
}

void
CoreAudioBackend::xrun_callback ()
{
	engine.Xrun ();
}

void
CoreAudioBackend::buffer_size_callback ()
{
	uint32_t bs = _pcmio->samples_per_period();
	if (bs == _samples_per_period) {
		return;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (_samples_per_period);
}

void
CoreAudioBackend::sample_rate_callback ()
{
	if (_preinit) {
#ifndef NDEBUG
		printf("Samplerate change during initialization.\n");
#endif
		return;
	}
	_pcmio->set_error_callback (NULL, NULL);
	_pcmio->set_sample_rate_callback (NULL, NULL);
	_pcmio->set_xrun_callback (NULL, NULL);
	_midiio->set_port_changed_callback(NULL, NULL);
	engine.halted_callback("Sample Rate Changed.");
	stop();
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

CoreAudioPort::CoreAudioPort (CoreAudioBackend &b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock (_buffer, sizeof (_buffer));

}

CoreAudioPort::~CoreAudioPort ()
{
}

void*
CoreAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		const std::set<BackendPortPtr>& connections = get_connections ();
		std::set<BackendPortPtr>::const_iterator it = connections.begin ();
		if (it == connections.end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			boost::shared_ptr<const CoreAudioPort> source = boost::dynamic_pointer_cast<const CoreAudioPort>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != connections.end ()) {
				source = boost::dynamic_pointer_cast<const CoreAudioPort>(*it);
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
	: BackendPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
	, _event (0, 0)
	, _first_time(true)
	, _unbuffered_bytes(0)
	, _total_bytes(0)
	, _expected_bytes(0)
	, _status_byte(0)

{
	_buffer[0].clear ();
	_buffer[1].clear ();

	_buffer[0].reserve (256);
	_buffer[1].reserve (256);
}

CoreMidiPort::~CoreMidiPort () { }

struct MidiEventSorter {
	bool operator() (CoreMidiEvent const& a, CoreMidiEvent const& b) {
		return a < b;
	}
};

void* CoreMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		const std::set<BackendPortPtr>& connections = get_connections ();
		for (std::set<BackendPortPtr>::const_iterator i = connections.begin ();
		     i != connections.end ();
		     ++i) {
			const CoreMidiBuffer * src = boost::dynamic_pointer_cast<const CoreMidiPort>(*i)->const_buffer ();
			for (CoreMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (*it);
			}
		}
		std::stable_sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}

	return &(_buffer[_bufperiod]);
}

int
CoreMidiPort::queue_event (
	void* port_buffer,
	pframes_t timestamp,
	const uint8_t* buffer, size_t size)
{
	const int ret = CoreAudioBackend::_midi_event_put (port_buffer, timestamp, buffer, size);
        if (!ret) { /* success */
                _event._pending = false;
        }
        return ret;
}

void
CoreMidiPort::reset_parser ()
{
	_event._pending = false;
	_first_time = true;
	_unbuffered_bytes = 0;
	_total_bytes = 0;
	_expected_bytes = 0;
	_status_byte = 0;
}

void
CoreMidiPort::clear_events ()
{
	CoreMidiBuffer* mbuf = static_cast<CoreMidiBuffer*>(get_buffer(0));
	mbuf->clear();
}

void
CoreMidiPort::parse_events (const uint64_t time, const uint8_t *data, const size_t size)
{
	CoreMidiBuffer* mbuf = static_cast<CoreMidiBuffer*>(get_buffer(0));

	if (_event._pending) {
		if (queue_event (mbuf, _event._time, _parser_buffer, _event._size)) {
			return;
		}
	}

	for (size_t i = 0; i < size; ++i) {
		if (_first_time && !(data[i] & 0x80)) {
			continue;
		}

		_first_time = false;

		if (process_byte(time, data[i])) {
			if (queue_event (mbuf, _event._time, _parser_buffer, _event._size)) {
				return;
			}
		}
	}
}

// based on JackMidiRawInputWriteQueue by Devin Anderson //
bool
CoreMidiPort::process_byte(const uint64_t time, const uint8_t byte)
{
	if (byte >= 0xf8) {
		// Realtime
		if (byte == 0xfd) {
			return false;
		}
		_parser_buffer[0] = byte;
		prepare_byte_event(time, byte);
		return true;
	}
	if (byte == 0xf7) {
		// Sysex end
		if (_status_byte == 0xf0) {
			record_byte(byte);
			return prepare_buffered_event(time);
		}
		_total_bytes = 0;
		_unbuffered_bytes = 0;
		_expected_bytes = 0;
		_status_byte = 0;
		return false;
	}
	if (byte >= 0x80) {
		// Non-realtime status byte
		if (_total_bytes) {
			printf ("CoreMidiPort: discarded bogus midi message\n");
#if 0
			for (size_t i=0; i < _total_bytes; ++i) {
				printf("%02x ", _parser_buffer[i]);
			}
			printf("\n");
#endif
			_total_bytes = 0;
			_unbuffered_bytes = 0;
		}
		_status_byte = byte;
		switch (byte & 0xf0) {
		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		case 0xe0:
			// Note On, Note Off, Aftertouch, Control Change, Pitch Wheel
			_expected_bytes = 3;
			break;
		case 0xc0:
		case 0xd0:
			// Program Change, Channel Pressure
			_expected_bytes = 2;
			break;
		case 0xf0:
			switch (byte) {
			case 0xf0:
				// Sysex
				_expected_bytes = 0;
				break;
			case 0xf1:
			case 0xf3:
				// MTC Quarter Frame, Song Select
				_expected_bytes = 2;
				break;
			case 0xf2:
				// Song Position
				_expected_bytes = 3;
				break;
			case 0xf4:
			case 0xf5:
				// Undefined
				_expected_bytes = 0;
				_status_byte = 0;
				return false;
			case 0xf6:
				// Tune Request
				prepare_byte_event(time, byte);
				_expected_bytes = 0;
				_status_byte = 0;
				return true;
			}
		}
		record_byte(byte);
		return false;
	}
	// Data byte
	if (! _status_byte) {
		// Data bytes without a status will be discarded.
		_total_bytes++;
		_unbuffered_bytes++;
		return false;
	}
	if (! _total_bytes) {
		record_byte(_status_byte);
	}
	record_byte(byte);
	return (_total_bytes == _expected_bytes) ? prepare_buffered_event(time) : false;
}


CoreMidiEvent::CoreMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
{
	if (size > 0 && size < MaxCoreMidiEventSize) {
		memcpy (_data, data, size);
	}
}

CoreMidiEvent::CoreMidiEvent (const CoreMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
{
	if (other._size > 0) {
		assert (other._size < MaxCoreMidiEventSize);
		memcpy (_data, other._data, other._size);
	}
};
