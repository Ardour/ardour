/*
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef PLATFORM_WINDOWS
#include <sys/mman.h>
#include <sys/time.h>
#endif

#ifdef COMPILER_MINGW
#include <sys/time.h>
#endif

#include <glibmm.h>

#include "portaudio_backend.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/microseconds.h"
#include "pbd/windows_timer_utils.h"
#include "pbd/windows_mmcss.h"

#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "pbd/i18n.h"

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
	, PortEngineSharedImpl (e, s_instance_name)
	, _pcmio (0)
	, _run (false)
	, _active (false)
	, _use_blocking_api(false)
	, _freewheel (false)
	, _freewheeling (false)
	, _freewheel_ack (false)
	, _reinit_thread_callback (false)
	, _measure_latency (false)
	, _cycle_count(0)
	, _total_deviation_us(0)
	, _max_deviation_us(0)
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
	pthread_mutex_init (&_freewheel_mutex, 0);
	pthread_cond_init (&_freewheel_signal, 0);

	_port_connection_queue.reserve (128);

	_pcmio = new PortAudioIO ();
	_midiio = new WinMMEMidiIO ();
}

PortAudioBackend::~PortAudioBackend ()
{
	delete _pcmio; _pcmio = 0;
	delete _midiio; _midiio = 0;

	clear_ports ();

	pthread_mutex_destroy (&_freewheel_mutex);
	pthread_cond_destroy (&_freewheel_signal);
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
	// update midi device info?
	return _pcmio->update_devices();
}

void
PortAudioBackend::set_use_buffered_io (bool use_buffered_io)
{
	DEBUG_AUDIO (string_compose ("Portaudio: use_buffered_io %1 \n", use_buffered_io));

	if (running()) {
		return;
	}

	_use_blocking_api = use_buffered_io;
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

int
PortAudioBackend::set_systemic_midi_input_latency (std::string const device, uint32_t sl)
{
	MidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) return -1;
	nfo->systemic_input_latency = sl;
	return 0;
}

int
PortAudioBackend::set_systemic_midi_output_latency (std::string const device, uint32_t sl)
{
	MidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) return -1;
	nfo->systemic_output_latency = sl;
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

uint32_t
PortAudioBackend::systemic_midi_input_latency (std::string const device) const
{
	MidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) return 0;
	return nfo->systemic_input_latency;
}

uint32_t
PortAudioBackend::systemic_midi_output_latency (std::string const device) const
{
	MidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) return 0;
	return nfo->systemic_output_latency;
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

std::vector<AudioBackend::DeviceStatus>
PortAudioBackend::enumerate_midi_devices () const
{
	std::vector<AudioBackend::DeviceStatus> midi_device_status;
	std::vector<MidiDeviceInfo*> device_info;

	if (_midi_driver_option == winmme_driver_name) {
		_midiio->update_device_info ();
		device_info = _midiio->get_device_info ();
	}

	for (std::vector<MidiDeviceInfo*>::const_iterator i = device_info.begin();
	     i != device_info.end();
	     ++i) {
		midi_device_status.push_back(DeviceStatus((*i)->device_name, true));
	}
	return midi_device_status;
}

MidiDeviceInfo*
PortAudioBackend::midi_device_info (const std::string& device_name) const
{
	std::vector<MidiDeviceInfo*> dev_info;

	if (_midi_driver_option == winmme_driver_name) {
		dev_info = _midiio->get_device_info();

		for (std::vector<MidiDeviceInfo*>::const_iterator i = dev_info.begin();
		     i != dev_info.end();
		     ++i) {
			if ((*i)->device_name == device_name) {
				return *i;
			}
		}
	}
	return 0;
}

int
PortAudioBackend::set_midi_device_enabled (std::string const device, bool enable)
{
	MidiDeviceInfo* nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->enable = enable;
	return 0;
}

bool
PortAudioBackend::midi_device_enabled (std::string const device) const
{
	MidiDeviceInfo* nfo = midi_device_info(device);
	if (!nfo) return false;
	return nfo->enable;
}

/* State Control */

static void * blocking_thread_func (void *arg)
{
	PortAudioBackend *d = static_cast<PortAudioBackend *>(arg);
	d->blocking_process_thread ();
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
		return BackendReinitializationError;
	}

	clear_ports ();

	/* reset internal state */
	assert (_run == false);
	_run = false;
	_dsp_load = 0;
	_freewheeling = false;
	_freewheel = false;

	PaErrorCode err = paNoError;

	if (_use_blocking_api) {
		DEBUG_AUDIO("Opening blocking audio stream\n");
		err = _pcmio->open_blocking_stream(name_to_id(_input_audio_device),
		                                   name_to_id(_output_audio_device),
		                                   _samplerate,
		                                   _samples_per_period);
	} else {
		DEBUG_AUDIO("Opening callback audio stream\n");
		err = _pcmio->open_callback_stream(name_to_id(_input_audio_device),
		                                   name_to_id(_output_audio_device),
		                                   _samplerate,
		                                   _samples_per_period,
		                                   portaudio_callback,
		                                   this);
	}

	// reintepret Portaudio error messages
	switch (err) {
	case paNoError:
		break;
	case paBadIODeviceCombination:
		return DeviceConfigurationNotSupportedError;
	case paInvalidChannelCount:
		return ChannelCountNotSupportedError;
	case paInvalidSampleRate:
		return SampleRateNotSupportedError;
	default:
		return AudioDeviceOpenError;
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

	if (_midi_driver_option == winmme_driver_name) {
		_midiio->set_enabled(true);
		//_midiio->set_port_changed_callback(midi_port_change, this);
		_midiio->start(); // triggers port discovery, callback coremidi_rediscover()
	}

	_cycle_timer.set_samplerate(_samplerate);
	_cycle_timer.set_samples_per_cycle(_samples_per_period);

	_dsp_calc.set_max_time_us (_cycle_timer.get_length_us());

	DEBUG_MIDI ("Registering MIDI ports\n");

	if (register_system_midi_ports () != 0) {
		DEBUG_PORTS("Failed to register system midi ports.\n")
		return PortRegistrationError;
	}

	DEBUG_AUDIO ("Registering Audio ports\n");

	if (register_system_audio_ports()) {
		DEBUG_PORTS("Failed to register system audio ports.\n");
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		DEBUG_PORTS("Could not re-establish ports.\n");
		return PortReconnectError;
	}

	_run = true;

	engine.reconnect_ports ();
	g_atomic_int_set (&_port_change_flag, 0);

	_dsp_calc.reset ();

	if (_use_blocking_api) {
		if (!start_blocking_process_thread()) {
			return ProcessThreadStartError;
		}
	} else {
		if (_pcmio->start_stream() != paNoError) {
			DEBUG_AUDIO("Unable to start stream\n");
			return AudioDeviceOpenError;
		}

		if (!start_freewheel_process_thread()) {
			DEBUG_AUDIO("Unable to start freewheel thread\n");
			stop();
			return ProcessThreadStartError;
		}

		/* wait for backend to become active */
		int timeout = 5000;
		while (!_active && --timeout > 0) { Glib::usleep (1000); }

		if (timeout == 0 || !_active) {
			PBD::error << _("PortAudio:: failed to start device.") << endmsg;
			stop ();
			return ProcessThreadStartError;
		}
	}

	return NoError;
}

int
PortAudioBackend::portaudio_callback(const void* input,
                                     void* output,
                                     unsigned long sample_count,
                                     const PaStreamCallbackTimeInfo* time_info,
                                     PaStreamCallbackFlags status_flags,
                                     void* user_data)
{
	PortAudioBackend* pa_backend = static_cast<PortAudioBackend*>(user_data);

	if (!pa_backend->process_callback((const float*)input,
	                                  (float*)output,
	                                  sample_count,
	                                  time_info,
	                                  status_flags)) {
		return paAbort;
	}

	return paContinue;
}

bool
PortAudioBackend::process_callback(const float* input,
                                   float* output,
                                   uint32_t sample_count,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags)
{
	PBD::WaitTimerRAII tr (dsp_stats[DeviceWait]);
	PBD::TimerRAII tr2 (dsp_stats[RunLoop]);

	_active = true;

	_dsp_calc.set_start_timestamp_us (PBD::get_microseconds());

	if (_run && _freewheel && !_freewheel_ack) {
		// acknowledge freewheeling; hand-over thread ID
		pthread_mutex_lock (&_freewheel_mutex);
		if (_freewheel) {
			DEBUG_AUDIO("Setting _freewheel_ack = true;\n");
			_freewheel_ack = true;
		}
		DEBUG_AUDIO("Signalling freewheel thread\n");
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}

	if (statusFlags & paInputUnderflow ||
		statusFlags & paInputOverflow ||
		statusFlags & paOutputUnderflow ||
		statusFlags & paOutputOverflow ) {
		DEBUG_AUDIO("PortAudio: Xrun\n");
		engine.Xrun();
		return true;
	}

	if (!_run || _freewheel) {
		memset(output, 0, sample_count * sizeof(float) * _system_outputs.size());
		return true;
	}

	bool in_main_thread = pthread_equal(_main_thread, pthread_self());

	if (_reinit_thread_callback || !in_main_thread) {
		_reinit_thread_callback = false;
		_main_thread = pthread_self();
		AudioEngine::thread_init_callback (this);
	}

	process_port_connection_changes();

	return blocking_process_main (input, output);
}

bool
PortAudioBackend::start_blocking_process_thread ()
{
	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MAIN, PBD_RT_STACKSIZE_PROC,
				&_main_blocking_thread, blocking_thread_func, this))
	{
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &_main_blocking_thread, blocking_thread_func, this))
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

	if (_use_blocking_api) {
		if (!stop_blocking_process_thread()) {
			return -1;
		}
	} else {
		_pcmio->close_stream();
		_active = false;

		if (!stop_freewheel_process_thread()) {
			return -1;
		}
	}

	unregister_ports();

	return (_active == false) ? 0 : -1;
}

static void* freewheel_thread(void* arg)
{
	PortAudioBackend* d = static_cast<PortAudioBackend*>(arg);
	d->freewheel_process_thread ();
	pthread_exit (0);
	return 0;
}

bool
PortAudioBackend::start_freewheel_process_thread ()
{
	if (pthread_create(&_pthread_freewheel, NULL, freewheel_thread, this)) {
		DEBUG_AUDIO("Failed to create main audio thread\n");
		return false;
	}

	int timeout = 5000;
	while (!_freewheel_thread_active && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_freewheel_thread_active) {
		DEBUG_AUDIO("Failed to start freewheel thread\n");
		return false;
	}
	return true;
}

bool
PortAudioBackend::stop_freewheel_process_thread ()
{
	void *status;

	if (!_freewheel_thread_active) {
		return true;
	}

	DEBUG_AUDIO("Signaling freewheel thread to stop\n");

	pthread_mutex_lock (&_freewheel_mutex);
	pthread_cond_signal (&_freewheel_signal);
	pthread_mutex_unlock (&_freewheel_mutex);

	if (pthread_join (_pthread_freewheel, &status) != 0) {
		DEBUG_AUDIO("Failed to stop freewheel thread\n");
		return false;
	}

	return true;
}

void*
PortAudioBackend::freewheel_process_thread()
{
	_freewheel_thread_active = true;

	bool first_run = false;

	pthread_mutex_lock (&_freewheel_mutex);

	while(_run) {
		// check if we should run,
		if (_freewheeling != _freewheel) {
			if (!_freewheeling) {
				DEBUG_AUDIO("Leaving freewheel\n");
				_freewheel = false; // first mark as disabled
				_reinit_thread_callback = true; // hand over _main_thread
				_freewheel_ack = false; // prepare next handshake
				_midiio->set_enabled(true);
				engine.freewheel_callback (_freewheeling);
				_dsp_calc.reset ();
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
			DEBUG_AUDIO("Waiting for freewheel change\n");
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
		}

		if (!blocking_process_freewheel()) {
			break;
		}

		process_port_connection_changes();
	}

	pthread_mutex_unlock (&_freewheel_mutex);

	_freewheel_thread_active = false;

	if (_run) {
		// engine.process_callback() returner error
		engine.halted_callback("CoreAudio Freehweeling aborted.");
	}
	return 0;
}

int
PortAudioBackend::freewheel (bool onoff)
{
	if (onoff == _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;

	if (0 == pthread_mutex_trylock (&_freewheel_mutex)) {
		pthread_cond_signal (&_freewheel_signal);
		pthread_mutex_unlock (&_freewheel_mutex);
	}
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
samplepos_t
PortAudioBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
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
	if (!_cycle_timer.valid()) {
		return 0;
	}

	return _cycle_timer.samples_since_cycle_start (PBD::get_microseconds());
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

bool
PortAudioBackend::set_mmcss_pro_audio (HANDLE* task_handle)
{
	bool mmcss_success = PBD::MMCSS::set_thread_characteristics ("Pro Audio", task_handle);

	if (!mmcss_success) {
		PBD::warning << get_error_string(SettingAudioThreadPriorityError) << endmsg;
		return false;
	} else {
		DEBUG_THREADS("Thread characteristics set to Pro Audio\n");
	}

	bool mmcss_priority =
		PBD::MMCSS::set_thread_priority(*task_handle, PBD::MMCSS::AVRT_PRIORITY_NORMAL);

	if (!mmcss_priority) {
		PBD::warning << get_error_string(SettingAudioThreadPriorityError) << endmsg;
		return false;
	} else {
		DEBUG_THREADS("Thread priority set to AVRT_PRIORITY_NORMAL\n");
	}

	return true;
}

bool
PortAudioBackend::reset_mmcss (HANDLE task_handle)
{
	if (!PBD::MMCSS::revert_thread_characteristics(task_handle)) {
		DEBUG_THREADS("Unable to reset process thread characteristics\n");
		return false;
	}
	return true;
}

void *
PortAudioBackend::portaudio_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;

#ifdef USE_MMCSS_THREAD_PRIORITIES
	HANDLE task_handle;
	bool mmcss_success = set_mmcss_pro_audio (&task_handle);
#endif

	DWORD tid = GetCurrentThreadId ();
	DEBUG_THREADS (string_compose ("Process Thread Child ID: %1\n", tid));

	f ();

#ifdef USE_MMCSS_THREAD_PRIORITIES
	if (mmcss_success) {
		reset_mmcss (task_handle);
	}
#endif

	return 0;
}

int
PortAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t   thread_id;
	ThreadData* td = new ThreadData (this, func, PBD_RT_STACKSIZE_PROC);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_PROC, PBD_RT_STACKSIZE_PROC,
				&thread_id, portaudio_process_thread, td)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &thread_id, portaudio_process_thread, td)) {
			DEBUG_AUDIO("Cannot create process thread.");
			return -1;
		}
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
	if (_use_blocking_api) {
		if (pthread_equal(_main_blocking_thread, pthread_self()) != 0) {
			return true;
		}
	} else {
		if (pthread_equal(_main_thread, pthread_self()) != 0) {
			return true;
		}
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

int
PortAudioBackend::register_system_audio_ports()
{
	LatencyRange lr;

	const uint32_t a_ins = _n_inputs;
	const uint32_t a_out = _n_outputs;

	uint32_t capture_latency = 0;
	uint32_t playback_latency = 0;

	// guard against erroneous latency values
	if (_pcmio->capture_latency() > _samples_per_period) {
		capture_latency = _pcmio->capture_latency() - _samples_per_period;
	}
	if (_pcmio->playback_latency() > _samples_per_period) {
		playback_latency = _pcmio->playback_latency() - _samples_per_period;
	}

	/* audio ports */
	lr.min = lr.max = capture_latency + (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (uint32_t i = 0; i < a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i+1);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		boost::shared_ptr<PortAudioPort> audio_port = boost::dynamic_pointer_cast<PortAudioPort>(p);
		audio_port->set_hw_port_name (
		    _pcmio->get_input_channel_name (name_to_id (_input_audio_device), i));
		_system_inputs.push_back (audio_port);
	}

	lr.min = lr.max = playback_latency + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (uint32_t i = 0; i < a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i+1);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		boost::shared_ptr<PortAudioPort> audio_port = boost::dynamic_pointer_cast<PortAudioPort>(p);
		audio_port->set_hw_port_name (
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
		std::string port_name = "system:midi_capture_" + (*i)->name();
		PortPtr p = add_port (port_name, DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));

		if (!p) {
			return -1;
		}

		MidiDeviceInfo* info = _midiio->get_device_info((*i)->name());
		if (info) { // assert?
			lr.min = lr.max = _samples_per_period + info->systemic_input_latency;
		}
		set_latency_range (p, false, lr);

		boost::shared_ptr<PortMidiPort> midi_port = boost::dynamic_pointer_cast<PortMidiPort>(p);
		midi_port->set_hw_port_name ((*i)->name());
		_system_midi_in.push_back (midi_port);
		DEBUG_MIDI (string_compose ("Registered MIDI input port: %1\n", port_name));
	}

	const std::vector<WinMMEMidiOutputDevice*> outputs = _midiio->get_outputs();

	for (std::vector<WinMMEMidiOutputDevice*>::const_iterator i = outputs.begin ();
	     i != outputs.end ();
	     ++i) {
		std::string port_name = "system:midi_playback_" + (*i)->name();
		PortPtr p = add_port (port_name, DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));

		if (!p) {
			return -1;
		}

		MidiDeviceInfo* info = _midiio->get_device_info((*i)->name());
		if (info) { // assert?
			lr.min = lr.max = _samples_per_period + info->systemic_output_latency;
		}
		set_latency_range (p, false, lr);

		boost::shared_ptr<PortMidiPort> midi_port = boost::dynamic_pointer_cast<PortMidiPort>(p);
		midi_port->set_n_periods(2);
		midi_port->set_hw_port_name ((*i)->name());
		_system_midi_out.push_back (midi_port);
		DEBUG_MIDI (string_compose ("Registered MIDI output port: %1\n", port_name));
	}
	return 0;
}

BackendPort*
PortAudioBackend::port_factory (std::string const & name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	BackendPort* port = 0;

	switch (type) {
		case DataType::AUDIO:
			port = new PortAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new PortMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << string_compose (_("%1::register_port: Invalid Data Type."), _instance_name) << endmsg;
			return 0;
	}

	return port;
}

/* MIDI */
int
PortAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t const** buf, void* port_buffer,
		uint32_t event_index)
{
	if (!buf || !port_buffer) return -1;
	PortMidiBuffer& source = * static_cast<PortMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	PortMidiEvent const& event = source[event_index];

	timestamp = event.timestamp ();
	size = event.size ();
	*buf = event.data ();
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
#ifndef NDEBUG
	if (dst.size () && (pframes_t)dst.back ().timestamp () > timestamp) {
		// nevermind, ::get_buffer() sorts events
		DEBUG_MIDI (string_compose ("PortMidiBuffer: unordered event: %1 > %2\n",
		                            (pframes_t)dst.back ().timestamp (),
		                            timestamp));
	}
#endif
	dst.push_back (PortMidiEvent (timestamp, buffer, size));
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
PortAudioBackend::set_latency_range (PortEngine::PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort>(port_handle);
	if (!valid_port (port)) {
		DEBUG_PORTS("BackendPort::set_latency_range (): invalid port.\n");
	}
	port->set_latency_range (latency_range, for_playback);
}

LatencyRange
PortAudioBackend::get_latency_range (PortEngine::PortHandle port_handle, bool for_playback)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort>(port_handle);
	LatencyRange r;
	if (!valid_port (port)) {
		DEBUG_PORTS("BackendPort::get_latency_range (): invalid port.\n");
		r.min = 0;
		r.max = 0;
		return r;
	}

	r = port->latency_range (for_playback);
	// TODO MIDI
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
PortAudioBackend::get_buffer (PortEngine::PortHandle port_handle, pframes_t nframes)
{
	boost::shared_ptr<BackendPort> port = boost::dynamic_pointer_cast<BackendPort>(port_handle);
	assert (port);
	assert (valid_port (port));
	if (!port || !valid_port (port)) return NULL; // XXX remove me
	return port->get_buffer (nframes);
}


void *
PortAudioBackend::blocking_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active = true;
	_processed_samples = 0;

	manager.registration_callback();
	manager.graph_order_callback();

	if (_pcmio->start_stream() != paNoError) {
		_pcmio->close_stream ();
		_active = false;
		engine.halted_callback(get_error_string(AudioDeviceIOError).c_str());
	}

#ifdef USE_MMCSS_THREAD_PRIORITIES
	HANDLE task_handle;
	bool mmcss_success = set_mmcss_pro_audio (&task_handle);
#endif

	DWORD tid = GetCurrentThreadId ();
	DEBUG_THREADS (string_compose ("Process Thread Master ID: %1\n", tid));

	_dsp_calc.reset ();
	while (_run) {

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
			if (!_freewheel) {
				_dsp_calc.reset ();
			}
		}

		if (!_freewheel) {

			dsp_stats[DeviceWait].start();
			int r = _pcmio->next_cycle (_samples_per_period);
			dsp_stats[DeviceWait].update();
			switch (r) {
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
	if (mmcss_success) {
		reset_mmcss(task_handle);
	}
#endif

	return 0;
}

bool
PortAudioBackend::blocking_process_main(const float* interleaved_input_data,
                                        float* interleaved_output_data)
{
	PBD::TimerRAII tr (dsp_stats[RunLoop]);
	uint32_t i = 0;
	int64_t min_elapsed_us = 1000000;
	int64_t max_elapsed_us = 0;

	_dsp_calc.set_start_timestamp_us (PBD::get_microseconds());

	i = 0;
	/* Copy input audio data into input port buffers */
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin();
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin();
	     it != _system_outputs.end();
	     ++it) {
		memset((*it)->get_buffer(_samples_per_period),
		       0,
		       _samples_per_period * sizeof(Sample));
	}

	_last_cycle_start = _cycle_timer.get_start();
	_cycle_timer.reset_start(PBD::get_microseconds());
	_cycle_count++;

	uint64_t cycle_diff_us = (_cycle_timer.get_start() - _last_cycle_start);
	int64_t deviation_us = (cycle_diff_us - _cycle_timer.get_length_us());
	_total_deviation_us += ::llabs(deviation_us);
	_max_deviation_us =
	    std::max(_max_deviation_us, (uint64_t)::llabs(deviation_us));

	if ((_cycle_count % 1000) == 0) {
		uint64_t mean_deviation_us = _total_deviation_us / _cycle_count;
		DEBUG_TIMING(string_compose("Mean avg cycle deviation: %1(ms), max %2(ms)\n",
		                            mean_deviation_us * 1e-3,
		                            _max_deviation_us * 1e-3));
	}

	if (::llabs(deviation_us) > _cycle_timer.get_length_us()) {
		DEBUG_TIMING(
		    string_compose("time between process(ms): %1, Est(ms): %2, Dev(ms): %3\n",
		                   cycle_diff_us * 1e-3,
		                   _cycle_timer.get_length_us() * 1e-3,
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin();
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
	_dsp_calc.set_stop_timestamp_us (PBD::get_microseconds());
	_dsp_load = _dsp_calc.get_dsp_load();

	DEBUG_TIMING(string_compose("DSP Load: %1\n", _dsp_load));

	max_elapsed_us = std::max(_dsp_calc.elapsed_time_us(), max_elapsed_us);
	min_elapsed_us = std::min(_dsp_calc.elapsed_time_us(), min_elapsed_us);
	if ((_cycle_count % 1000) == 0) {
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin();
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin();
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
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin();
	     it != _system_midi_in.end();
	     ++it, ++i) {
		PortMidiBuffer* mbuf = static_cast<PortMidiBuffer*>((*it)->get_buffer(0));
		mbuf->clear();
		uint64_t timestamp;
		pframes_t sample_offset;
		uint8_t data[MaxWinMidiEventSize];
		size_t size = sizeof(data);
		while (_midiio->dequeue_input_event(i,
		                                    _cycle_timer.get_start(),
		                                    _cycle_timer.get_next_start(),
		                                    timestamp,
		                                    data,
		                                    size)) {
			sample_offset = _cycle_timer.samples_since_cycle_start(timestamp);
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
	for (std::vector<BackendPortPtr>::iterator it = _system_midi_out.begin();
	     it != _system_midi_out.end();
	     ++it) {
		boost::dynamic_pointer_cast<PortMidiPort>(*it)->next_period();
	}
	/* queue outgoing midi */
	uint32_t i = 0;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin();
	     it != _system_midi_out.end();
	     ++it, ++i) {
		const PortMidiBuffer* src =
			boost::dynamic_pointer_cast<const PortMidiPort>(*it)->const_buffer();

		for (PortMidiBuffer::const_iterator mit = src->begin(); mit != src->end();
		     ++mit) {
			uint64_t timestamp =
			    _cycle_timer.timestamp_from_sample_offset(mit->timestamp());
			DEBUG_MIDI(string_compose("Queuing outgoing MIDI data for device: "
			                          "%1 sample_offset: %2 timestamp: %3, size: %4\n",
			                          _midiio->get_outputs()[i]->name(),
			                          mit->timestamp(),
			                          timestamp,
			                          mit->size()));
			_midiio->enqueue_output_event(i, timestamp, mit->data(), mit->size());
		}
	}
}

void
PortAudioBackend::process_port_connection_changes ()
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

/******************************************************************************/

static boost::shared_ptr<PortAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	BACKEND_NAME,
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

/******************************************************************************/

PortAudioPort::PortAudioPort (PortAudioBackend &b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
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
		std::set<BackendPortPtr>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			boost::shared_ptr<const PortAudioPort> source = boost::dynamic_pointer_cast<const PortAudioPort>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = boost::dynamic_pointer_cast<const PortAudioPort>(*it);
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
	: BackendPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();

	_buffer[0].reserve (256);
	_buffer[1].reserve (256);
}

PortMidiPort::~PortMidiPort () { }

struct MidiEventSorter {
	bool operator() (PortMidiEvent const& a, PortMidiEvent const& b) {
		return a < b;
	}
};

void* PortMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		for (std::set<BackendPortPtr>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const PortMidiBuffer * src = boost::dynamic_pointer_cast<const PortMidiPort>(*i)->const_buffer ();
			for (PortMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (*it);
			}
		}
		std::stable_sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}
	return &(_buffer[_bufperiod]);
}

PortMidiEvent::PortMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
{
	if (size > 0 && size < MaxWinMidiEventSize) {
		memcpy (_data, data, size);
	}
}

PortMidiEvent::PortMidiEvent (const PortMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
{
	if (other._size > 0) {
		assert (other._size < MaxWinMidiEventSize);
		memcpy (_data, other._data, other._size);
	}
};
