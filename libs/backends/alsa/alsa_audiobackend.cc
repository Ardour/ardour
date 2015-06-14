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

#include "alsa_audiobackend.h"
#include "rt_thread.h"

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "ardouralsautil/devicelist.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t AlsaAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> AlsaAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_input_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_output_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_duplex_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_midi_device_status;

AlsaAudioBackend::AlsaAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _pcmi (0)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _freewheeling (false)
	, _measure_latency (false)
	, _last_process_start (0)
	, _input_audio_device("")
	, _output_audio_device("")
	, _midi_driver_option(_("None"))
	, _device_reservation(0)
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _periods_per_cycle (2)
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
}

AlsaAudioBackend::~AlsaAudioBackend ()
{
	pthread_mutex_destroy (&_port_callback_mutex);
}

/* AUDIOBACKEND API */

std::string
AlsaAudioBackend::name () const
{
	return X_("ALSA");
}

bool
AlsaAudioBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_devices () const
{
	_duplex_audio_device_status.clear();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names(devices);
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_input_audio_device == "") _input_audio_device = i->first;
		if (_output_audio_device == "") _output_audio_device = i->first;
		_duplex_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _duplex_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_input_devices () const
{
	_input_audio_device_status.clear();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names(devices, HalfDuplexIn);
	_input_audio_device_status.push_back (DeviceStatus (_("None"), true));
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_input_audio_device == "") _input_audio_device = i->first;
		_input_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _input_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_output_devices () const
{
	_output_audio_device_status.clear();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names(devices, HalfDuplexOut);
	_output_audio_device_status.push_back (DeviceStatus (_("None"), true));
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (_output_audio_device == "") _output_audio_device = i->first;
		_output_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _output_audio_device_status;
}

void
AlsaAudioBackend::reservation_stdout (std::string d, size_t /* s */)
{
  if (d.substr(0, 19) == "Acquired audio-card") {
		_reservation_succeeded = true;
	}
}

void
AlsaAudioBackend::release_device()
{
	_reservation_connection.drop_connections();
	ARDOUR::SystemExec * tmp = _device_reservation;
	_device_reservation = 0;
	delete tmp;
}

bool
AlsaAudioBackend::acquire_device(const char* device_name)
{
	/* This is  quick hack, ideally we'll link against libdbus and implement a dbus-listener
	 * that owns the device. here we try to get away by just requesting it and then block it...
	 * (pulseaudio periodically checks anyway)
	 *
	 * dbus-send --session --print-reply --type=method_call --dest=org.freedesktop.ReserveDevice1.Audio2 /org/freedesktop/ReserveDevice1/Audio2 org.freedesktop.ReserveDevice1.RequestRelease int32:4
	 * -> should not return  'boolean false'
	 */
	int device_number = card_to_num(device_name);
	if (device_number < 0) return false;

	assert(_device_reservation == 0);
	_reservation_succeeded = false;

	std::string request_device_exe;
	if (!PBD::find_file (
				PBD::Searchpath(Glib::build_filename(ARDOUR::ardour_dll_directory(), "ardouralsautil")
					+ G_SEARCHPATH_SEPARATOR_S + ARDOUR::ardour_dll_directory()),
				"ardour-request-device", request_device_exe))
	{
		PBD::warning << "ardour-request-device binary was not found..'" << endmsg;
		return false;
	}
	else
	{
		char **argp;
		char tmp[128];
		argp=(char**) calloc(5,sizeof(char*));
		argp[0] = strdup(request_device_exe.c_str());
		argp[1] = strdup("-P");
		snprintf(tmp, sizeof(tmp), "%d", getpid());
		argp[2] = strdup(tmp);
		snprintf(tmp, sizeof(tmp), "Audio%d", device_number);
		argp[3] = strdup(tmp);
		argp[4] = 0;

		_device_reservation = new ARDOUR::SystemExec(request_device_exe, argp);
		_device_reservation->ReadStdout.connect_same_thread (_reservation_connection, boost::bind (&AlsaAudioBackend::reservation_stdout, this, _1 ,_2));
		_device_reservation->Terminated.connect_same_thread (_reservation_connection, boost::bind (&AlsaAudioBackend::release_device, this));
		if (_device_reservation->start(0)) {
			PBD::warning << _("AlsaAudioBackend: Device Request failed.") << endmsg;
			release_device();
			return false;
		}
	}
	// wait to check if reservation suceeded.
	int timeout = 500; // 5 sec
	while (_device_reservation && !_reservation_succeeded && --timeout > 0) {
		Glib::usleep(10000);
	}
	if (timeout == 0 || !_reservation_succeeded) {
		PBD::warning << _("AlsaAudioBackend: Device Reservation failed.") << endmsg;
		release_device();
		return false;
	}
	return true;
}

std::vector<float>
AlsaAudioBackend::available_sample_rates (const std::string&) const
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
AlsaAudioBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
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
AlsaAudioBackend::available_input_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

uint32_t
AlsaAudioBackend::available_output_channel_count (const std::string&) const
{
	return 128; // TODO query current device
}

bool
AlsaAudioBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
AlsaAudioBackend::can_change_buffer_size_when_running () const
{
	return false; // why not? :)
}

int
AlsaAudioBackend::set_input_device_name (const std::string& d)
{
	_input_audio_device = d;
	return 0;
}

int
AlsaAudioBackend::set_output_device_name (const std::string& d)
{
	_output_audio_device = d;
	return 0;
}

int
AlsaAudioBackend::set_device_name (const std::string& d)
{
	int rv = 0;
	rv |= set_input_device_name (d);
	rv |= set_output_device_name (d);
	return rv;
}

int
AlsaAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) { return -1; }
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
AlsaAudioBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	if (_run) {
		return -1;
	}
	_samples_per_period = bs;
	engine.buffer_size_change (bs);
	return 0;
}

int
AlsaAudioBackend::set_interleaved (bool yn)
{
	if (!yn) { return 0; }
	return -1;
}

int
AlsaAudioBackend::set_input_channels (uint32_t cc)
{
	_n_inputs = cc;
	return 0;
}

int
AlsaAudioBackend::set_output_channels (uint32_t cc)
{
	_n_outputs = cc;
	return 0;
}

int
AlsaAudioBackend::set_systemic_input_latency (uint32_t sl)
{
	_systemic_audio_input_latency = sl;
	return 0;
}

int
AlsaAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	_systemic_audio_output_latency = sl;
	return 0;
}

int
AlsaAudioBackend::set_systemic_midi_input_latency (std::string const device, uint32_t sl)
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->systemic_input_latency = sl;
	return 0;
}

int
AlsaAudioBackend::set_systemic_midi_output_latency (std::string const device, uint32_t sl)
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->systemic_output_latency = sl;
	return 0;
}

/* Retrieving parameters */
std::string
AlsaAudioBackend::device_name () const
{
	if (_input_audio_device != _("None")) {
		return _input_audio_device;
	}
	if (_output_audio_device != _("None")) {
		return _output_audio_device;
	}
	return "";
}

std::string
AlsaAudioBackend::input_device_name () const
{
	return _input_audio_device;
}

std::string
AlsaAudioBackend::output_device_name () const
{
	return _output_audio_device;
}

float
AlsaAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
AlsaAudioBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
AlsaAudioBackend::interleaved () const
{
	return false;
}

uint32_t
AlsaAudioBackend::input_channels () const
{
	return _n_inputs;
}

uint32_t
AlsaAudioBackend::output_channels () const
{
	return _n_outputs;
}

uint32_t
AlsaAudioBackend::systemic_input_latency () const
{
	return _systemic_audio_input_latency;
}

uint32_t
AlsaAudioBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

uint32_t
AlsaAudioBackend::systemic_midi_input_latency (std::string const device) const
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return 0;
	return nfo->systemic_input_latency;
}

uint32_t
AlsaAudioBackend::systemic_midi_output_latency (std::string const device) const
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return 0;
	return nfo->systemic_output_latency;
}

/* MIDI */
struct AlsaAudioBackend::AlsaMidiDeviceInfo *
AlsaAudioBackend::midi_device_info(std::string const name) const {
	for (std::map<std::string, struct AlsaMidiDeviceInfo*>::const_iterator i = _midi_devices.begin (); i != _midi_devices.end(); ++i) {
		if (i->first == name) {
			return (i->second);
		}
	}

	assert(_midi_driver_option != _("None"));

	std::map<std::string, std::string> devices;
	if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names(devices);
	} else {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (i->first == name) {
			_midi_devices[name] = new AlsaMidiDeviceInfo();
			return _midi_devices[name];
		}
	}
	return 0;
}

std::vector<std::string>
AlsaAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty()) {
		_midi_options.push_back (_("ALSA raw devices"));
		_midi_options.push_back (_("ALSA sequencer"));
		_midi_options.push_back (_("None"));
	}
	return _midi_options;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_midi_devices () const
{
	_midi_device_status.clear();
	std::map<std::string, std::string> devices;

	if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names (devices);
	}
	else if (_midi_driver_option == _("ALSA sequencer")) {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		_midi_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _midi_device_status;
}

int
AlsaAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != _("None") && opt != _("ALSA raw devices") && opt != _("ALSA sequencer")) {
		return -1;
	}
	_midi_driver_option = opt;
	return 0;
}

std::string
AlsaAudioBackend::midi_option () const
{
	return _midi_driver_option;
}

int
AlsaAudioBackend::set_midi_device_enabled (std::string const device, bool enable)
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return -1;
	nfo->enabled = enable;
	return 0;
}

bool
AlsaAudioBackend::midi_device_enabled (std::string const device) const
{
	struct AlsaMidiDeviceInfo * nfo = midi_device_info(device);
	if (!nfo) return false;
	return nfo->enabled;
}

/* State Control */

static void * pthread_process (void *arg)
{
	AlsaAudioBackend *d = static_cast<AlsaAudioBackend *>(arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
AlsaAudioBackend::_start (bool for_latency_measurement)
{
	if (!_active && _run) {
		// recover from 'halted', reap threads
		stop();
	}

	if (_active || _run) {
		PBD::error << _("AlsaAudioBackend: already active.") << endmsg;
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("AlsaAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
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

	release_device();

	assert(_rmidi_in.size() == 0);
	assert(_rmidi_out.size() == 0);
	assert(_pcmi == 0);

	int duplex = 0;
	std::string audio_device;
	std::string alsa_device;
	std::map<std::string, std::string> devices;

	if (_input_audio_device == _("None") && _output_audio_device == _("None")) {
		PBD::error << _("AlsaAudioBackend: At least one of input or output device needs to be set.");
		return -1;
	}

	if (_input_audio_device != _output_audio_device) {
		if (_input_audio_device != _("None") && _output_audio_device != _("None")) {
			PBD::error << _("AlsaAudioBackend: Cannot use two different devices.");
			return -1;
		}
		if (_input_audio_device != _("None")) {
			get_alsa_audio_device_names(devices, HalfDuplexIn);
			audio_device = _input_audio_device;
			duplex = 1;
		} else {
			get_alsa_audio_device_names(devices, HalfDuplexOut);
			audio_device = _output_audio_device;
			duplex = 2;
		}
	} else {
		get_alsa_audio_device_names(devices);
		audio_device = _input_audio_device;
		duplex = 3;
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (i->first == audio_device) {
			alsa_device = i->second;
			break;
		}
	}
	if (alsa_device == "") {
		PBD::error << _("AlsaAudioBackend: Cannot find configured device. Is it still connected?");
		return -1;
	}

	acquire_device(alsa_device.c_str());
	_pcmi = new Alsa_pcmi (
			(duplex & 2) ? alsa_device.c_str() : NULL,
			(duplex & 1) ? alsa_device.c_str() : NULL,
			0, _samplerate, _samples_per_period, _periods_per_cycle, 0);
	switch (_pcmi->state ()) {
		case 0: /* OK */ break;
		case -1: PBD::error << _("AlsaAudioBackend: failed to open device.") << endmsg; break;
		case -2: PBD::error << _("AlsaAudioBackend: failed to allocate parameters.") << endmsg; break;
		case -3: PBD::error << _("AlsaAudioBackend: cannot set requested sample rate.") << endmsg; break;
		case -4: PBD::error << _("AlsaAudioBackend: cannot set requested period size.") << endmsg; break;
		case -5: PBD::error << _("AlsaAudioBackend: cannot set requested number of periods.") << endmsg; break;
		case -6: PBD::error << _("AlsaAudioBackend: unsupported sample format.") << endmsg; break;
		default: PBD::error << _("AlsaAudioBackend: initialization failed.") << endmsg; break;
	}
	if (_pcmi->state ()) {
		delete _pcmi; _pcmi = 0;
		release_device();
		return -1;
	}

#ifndef NDEBUG
	_pcmi->printinfo ();
#endif

	if (_n_outputs != _pcmi->nplay ()) {
		if (_n_outputs == 0) {
		 _n_outputs = _pcmi->nplay ();
		} else {
		 _n_outputs = std::min (_n_outputs, _pcmi->nplay ());
		}
		PBD::warning << _("AlsaAudioBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmi->ncapt ()) {
		if (_n_inputs == 0) {
		 _n_inputs = _pcmi->ncapt ();
		} else {
		 _n_inputs = std::min (_n_inputs, _pcmi->ncapt ());
		}
		PBD::warning << _("AlsaAudioBackend: adjusted input channel count to match device.") << endmsg;
	}

	if (_pcmi->fsize() != _samples_per_period) {
		_samples_per_period = _pcmi->fsize();
		PBD::warning << _("AlsaAudioBackend: samples per period does not match.") << endmsg;
	}

	if (_pcmi->fsamp() != _samplerate) {
		_samplerate = _pcmi->fsamp();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("AlsaAudioBackend: sample rate does not match.") << endmsg;
	}

	_measure_latency = for_latency_measurement;

	register_system_midi_ports();

	if (register_system_audio_ports()) {
		PBD::error << _("AlsaAudioBackend: failed to register system ports.") << endmsg;
		delete _pcmi; _pcmi = 0;
		release_device();
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("AlsaAudioBackend: Could not re-establish ports.") << endmsg;
		delete _pcmi; _pcmi = 0;
		release_device();
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
			PBD::error << _("AlsaAudioBackend: failed to create process thread.") << endmsg;
			delete _pcmi; _pcmi = 0;
			release_device();
			_run = false;
			return -1;
		} else {
			PBD::warning << _("AlsaAudioBackend: cannot acquire realtime permissions.") << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_active) {
		PBD::error << _("AlsaAudioBackend: failed to start process thread.") << endmsg;
		delete _pcmi; _pcmi = 0;
		release_device();
		_run = false;
		return -1;
	}

	return 0;
}

int
AlsaAudioBackend::stop ()
{
	void *status;
	if (!_run) {
		return 0;
	}

	_run = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("AlsaAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}

	while (!_rmidi_out.empty ()) {
		AlsaMidiIO *m = _rmidi_out.back ();
		m->stop();
		_rmidi_out.pop_back ();
		delete m;
	}
	while (!_rmidi_in.empty ()) {
		AlsaMidiIO *m = _rmidi_in.back ();
		m->stop();
		_rmidi_in.pop_back ();
		delete m;
	}

	unregister_ports();
	delete _pcmi; _pcmi = 0;
	release_device();

	return (_active == false) ? 0 : -1;
}

int
AlsaAudioBackend::freewheel (bool onoff)
{
	_freewheeling = onoff;
	return 0;
}

float
AlsaAudioBackend::dsp_load () const
{
	return std::min(100.f, 100.f * _dsp_load);
}

size_t
AlsaAudioBackend::raw_buffer_size (DataType t)
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
AlsaAudioBackend::sample_time ()
{
	return _processed_samples;
}

framepos_t
AlsaAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
AlsaAudioBackend::samples_since_cycle_start ()
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


void *
AlsaAudioBackend::alsa_process_thread (void *arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
AlsaAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t thread_id;
	pthread_attr_t attr;
	size_t stacksize = 100000;

	ThreadData* td = new ThreadData (this, func, stacksize);

	if (_realtime_pthread_create (SCHED_FIFO, -21, stacksize,
				&thread_id, alsa_process_thread, td)) {
		pthread_attr_init (&attr);
		pthread_attr_setstacksize (&attr, stacksize);
		if (pthread_create (&thread_id, &attr, alsa_process_thread, td)) {
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
AlsaAudioBackend::join_process_threads ()
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
AlsaAudioBackend::in_process_thread ()
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
AlsaAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
AlsaAudioBackend::update_latencies ()
{
	// trigger latency callback in RT thread (locked graph)
	port_connect_add_remove_callback();
}

/* PORTENGINE API */

void*
AlsaAudioBackend::private_handle () const
{
	return NULL;
}

const std::string&
AlsaAudioBackend::my_name () const
{
	return _instance_name;
}

bool
AlsaAudioBackend::available () const
{
	return _run && _active;
}

uint32_t
AlsaAudioBackend::port_name_size () const
{
	return 256;
}

int
AlsaAudioBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<AlsaPort*>(port)->set_name (_instance_name + ":" + name);
}

std::string
AlsaAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<AlsaPort*>(port)->name ();
}

PortEngine::PortHandle
AlsaAudioBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle) find_port (name);
	return port;
}

int
AlsaAudioBackend::get_ports (
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
		AlsaPort* port = _ports[i];
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
AlsaAudioBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<AlsaPort*>(port)->type ();
}

PortEngine::PortHandle
AlsaAudioBackend::register_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	if (name.size () == 0) { return 0; }
	if (flags & IsPhysical) { return 0; }
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
AlsaAudioBackend::add_port (
		const std::string& name,
		ARDOUR::DataType type,
		ARDOUR::PortFlags flags)
{
	assert(name.size ());
	if (find_port (name)) {
		PBD::error << _("AlsaBackend::register_port: Port already exists:")
				<< " (" << name << ")" << endmsg;
		return 0;
	}
	AlsaPort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new AlsaAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new AlsaMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << _("AlsaBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.push_back (port);

	return port;
}

void
AlsaAudioBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!_run) {
		return;
	}
	AlsaPort* port = static_cast<AlsaPort*>(port_handle);
	std::vector<AlsaPort*>::iterator i = std::find (_ports.begin (), _ports.end (), static_cast<AlsaPort*>(port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("AlsaBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all(port_handle);
	_ports.erase (i);
	delete port;
}

int
AlsaAudioBackend::register_system_audio_ports()
{
	LatencyRange lr;

	const int a_ins = _n_inputs;
	const int a_out = _n_outputs;

	/* audio ports */
	lr.min = lr.max = (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (int i = 1; i <= a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		_system_inputs.push_back(static_cast<AlsaPort*>(p));
	}

	lr.min = lr.max = (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_outputs.push_back(static_cast<AlsaPort*>(p));
	}
	return 0;
}

int
AlsaAudioBackend::register_system_midi_ports()
{
	std::map<std::string, std::string> devices;
	int midi_ins = 0;
	int midi_outs = 0;

	if (_midi_driver_option == _("None")) {
		return 0;
	} else if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names(devices);
	} else {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		struct AlsaMidiDeviceInfo * nfo = midi_device_info(i->first);
		if (!nfo) continue;
		if (!nfo->enabled) continue;

		AlsaMidiOut *mout;
		if (_midi_driver_option == _("ALSA raw devices")) {
			mout = new AlsaRawMidiOut (i->second.c_str());
		} else {
			mout = new AlsaSeqMidiOut (i->second.c_str());
		}

		if (mout->state ()) {
			PBD::warning << string_compose (
					_("AlsaMidiOut: failed to open midi device '%1'."), i->second)
				<< endmsg;
			delete mout;
		} else {
			mout->setup_timing(_samples_per_period, _samplerate);
			mout->sync_time (g_get_monotonic_time());
			if (mout->start ()) {
				PBD::warning << string_compose (
						_("AlsaMidiOut: failed to start midi device '%1'."), i->second)
					<< endmsg;
				delete mout;
			} else {
				char tmp[64];
				snprintf(tmp, sizeof(tmp), "system:midi_playback_%d", ++midi_ins);
				PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
				if (!p) {
					mout->stop();
					delete mout;
				}
				LatencyRange lr;
				lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_output_latency);
				set_latency_range (p, false, lr);
				static_cast<AlsaMidiPort*>(p)->set_n_periods(2);
				_system_midi_out.push_back(static_cast<AlsaPort*>(p));
				_rmidi_out.push_back (mout);
			}
		}

		AlsaMidiIn *midin;
		if (_midi_driver_option == _("ALSA raw devices")) {
			midin = new AlsaRawMidiIn (i->second.c_str());
		} else {
			midin = new AlsaSeqMidiIn (i->second.c_str());
		}

		if (midin->state ()) {
			PBD::warning << string_compose (
					_("AlsaMidiIn: failed to open midi device '%1'."), i->second)
				<< endmsg;
			delete midin;
		} else {
			midin->setup_timing(_samples_per_period, _samplerate);
			midin->sync_time (g_get_monotonic_time());
			if (midin->start ()) {
				PBD::warning << string_compose (
						_("AlsaMidiIn: failed to start midi device '%1'."), i->second)
					<< endmsg;
				delete midin;
			} else {
				char tmp[64];
				snprintf(tmp, sizeof(tmp), "system:midi_capture_%d", ++midi_outs);
				PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
				if (!p) {
					midin->stop();
					delete midin;
					continue;
				}
				LatencyRange lr;
				lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_input_latency);
				set_latency_range (p, false, lr);
				_system_midi_in.push_back(static_cast<AlsaPort*>(p));
				_rmidi_in.push_back (midin);
			}
		}
	}
	return 0;
}

void
AlsaAudioBackend::unregister_ports (bool system_only)
{
	size_t i = 0;
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();
	while (i <  _ports.size ()) {
		AlsaPort* port = _ports[i];
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
AlsaAudioBackend::connect (const std::string& src, const std::string& dst)
{
	AlsaPort* src_port = find_port (src);
	AlsaPort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Source port:")
				<< " (" << src <<")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Destination port:")
			<< " (" << dst <<")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
AlsaAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	AlsaPort* src_port = find_port (src);
	AlsaPort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("AlsaBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
AlsaAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	AlsaPort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("AlsaBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("AlsaBackend::connect: Invalid Destination Port")
			<< " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<AlsaPort*>(src)->connect (dst_port);
}

int
AlsaAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	AlsaPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("AlsaBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<AlsaPort*>(src)->disconnect (dst_port);
}

int
AlsaAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<AlsaPort*>(port)->disconnect_all ();
	return 0;
}

bool
AlsaAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<AlsaPort*>(port)->is_connected ();
}

bool
AlsaAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	AlsaPort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("AlsaBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<AlsaPort*>(src)->is_connected (dst_port);
}

bool
AlsaAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<AlsaPort*>(port)->is_physically_connected ();
}

int
AlsaAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::vector<AlsaPort*>& connected_ports = static_cast<AlsaPort*>(port)->get_connections ();

	for (std::vector<AlsaPort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
AlsaAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t** buf, void* port_buffer,
		uint32_t event_index)
{
	assert (buf && port_buffer);
	AlsaMidiBuffer& source = * static_cast<AlsaMidiBuffer*>(port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	AlsaMidiEvent * const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size = event->size ();
	*buf = event->data ();
	return 0;
}

int
AlsaAudioBackend::midi_event_put (
		void* port_buffer,
		pframes_t timestamp,
		const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	AlsaMidiBuffer& dst = * static_cast<AlsaMidiBuffer*>(port_buffer);
	if (dst.size () && (pframes_t)dst.back ()->timestamp () > timestamp) {
#ifndef NDEBUG
		// nevermind, ::get_buffer() sorts events
		fprintf (stderr, "AlsaMidiBuffer: it's too late for this event. %d > %d\n",
				(pframes_t)dst.back ()->timestamp (), timestamp);
#endif
	}
	dst.push_back (boost::shared_ptr<AlsaMidiEvent>(new AlsaMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
AlsaAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<AlsaMidiBuffer*>(port_buffer)->size ();
}

void
AlsaAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
	AlsaMidiBuffer * buf = static_cast<AlsaMidiBuffer*>(port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
AlsaAudioBackend::can_monitor_input () const
{
	return false;
}

int
AlsaAudioBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
AlsaAudioBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
AlsaAudioBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
AlsaAudioBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaPort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<AlsaPort*>(port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
AlsaAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::error << _("AlsaPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}
	AlsaPort *p = static_cast<AlsaPort*>(port);
	assert(p);

	r = p->latency_range (for_playback);
	if (p->is_physical() && p->is_terminal()) {
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
AlsaAudioBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("AlsaPort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<AlsaPort*>(port)->is_physical ();
}

void
AlsaAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		AlsaPort* port = _ports[i];
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
AlsaAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		AlsaPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

ChanCount
AlsaAudioBackend::n_physical_outputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		AlsaPort* port = _ports[i];
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
AlsaAudioBackend::n_physical_inputs () const
{
	int n_midi = 0;
	int n_audio = 0;
	for (size_t i = 0; i < _ports.size (); ++i) {
		AlsaPort* port = _ports[i];
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
AlsaAudioBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	assert (port);
	assert (valid_port (port));
	return static_cast<AlsaPort*>(port)->get_buffer (nframes);
}

/* Engine Process */
void *
AlsaAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active = true;
	_processed_samples = 0;

	uint64_t clock1, clock2;
	_pcmi->pcm_start ();
	int no_proc_errors = 0;
	const int bailout = 2 * _samplerate / _samples_per_period;
	const int64_t nominal_time = 1e6 * _samples_per_period / _samplerate;

	manager.registration_callback();
	manager.graph_order_callback();

	while (_run) {
		long nr;
		bool xrun = false;

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
		}

		if (!_freewheel) {
			nr = _pcmi->pcm_wait ();

			if (_pcmi->state () > 0) {
				++no_proc_errors;
				xrun = true;
			}
			if (_pcmi->state () < 0) {
				PBD::error << _("AlsaAudioBackend: I/O error. Audio Process Terminated.") << endmsg;
				break;
			}
			if (no_proc_errors > bailout) {
				PBD::error
					<< string_compose (
							_("AlsaAudioBackend: Audio Process Terminated after %1 consecutive x-runs."),
							no_proc_errors)
					<< endmsg;
				break;
			}

			while (nr >= (long)_samples_per_period && _freewheeling == _freewheel) {
				uint32_t i = 0;
				clock1 = g_get_monotonic_time();
				no_proc_errors = 0;

				_pcmi->capt_init (_samples_per_period);
				for (std::vector<AlsaPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++i) {
					_pcmi->capt_chan (i, (float*)((*it)->get_buffer(_samples_per_period)), _samples_per_period);
				}
				_pcmi->capt_done (_samples_per_period);

				/* de-queue incoming midi*/
				i = 0;
				for (std::vector<AlsaPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
					assert (_rmidi_in.size() > i);
					AlsaMidiIn *rm = _rmidi_in.at(i);
					void *bptr = (*it)->get_buffer(0);
					pframes_t time;
					uint8_t data[64]; // match MaxAlsaEventSize in alsa_rawmidi.cc
					size_t size = sizeof(data);
					midi_clear(bptr);
					while (rm->recv_event (time, data, size)) {
						midi_event_put(bptr, time, data, size);
						size = sizeof(data);
					}
					rm->sync_time (clock1);
				}

				for (std::vector<AlsaPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
					memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
				}

				/* call engine process callback */
				_last_process_start = g_get_monotonic_time();
				if (engine.process_callback (_samples_per_period)) {
					_pcmi->pcm_stop ();
					_active = false;
					return 0;
				}

				for (std::vector<AlsaPort*>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
					static_cast<AlsaMidiPort*>(*it)->next_period();
				}

				/* queue outgoing midi */
				i = 0;
				for (std::vector<AlsaPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
					assert (_rmidi_out.size() > i);
					const AlsaMidiBuffer * src = static_cast<const AlsaMidiPort*>(*it)->const_buffer();
					AlsaMidiOut *rm = _rmidi_out.at(i);
					rm->sync_time (clock1);
					for (AlsaMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
						rm->send_event ((*mit)->timestamp(), (*mit)->data(), (*mit)->size());
					}
				}

				/* write back audio */
				i = 0;
				_pcmi->play_init (_samples_per_period);
				for (std::vector<AlsaPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
					_pcmi->play_chan (i, (const float*)(*it)->get_buffer (_samples_per_period), _samples_per_period);
				}
				for (; i < _pcmi->nplay (); ++i) {
					_pcmi->clear_chan (i, _samples_per_period);
				}
				_pcmi->play_done (_samples_per_period);
				nr -= _samples_per_period;
				_processed_samples += _samples_per_period;

				/* calculate DSP load */
				clock2 = g_get_monotonic_time();
				const int64_t elapsed_time = clock2 - clock1;
				// low pass filter
				const float load = elapsed_time / (float) nominal_time;
				if (load > _dsp_load) {
					_dsp_load = load;
				} else {
					const float a = .2 * _samples_per_period / _samplerate;
					_dsp_load = _dsp_load + a * (load - _dsp_load) + 1e-12;
				}
			}

			if (xrun && (_pcmi->capt_xrun() > 0 || _pcmi->play_xrun() > 0)) {
				engine.Xrun ();
#if 0
				fprintf(stderr, "ALSA x-run read: %.2f ms, write: %.2f ms\n",
						_pcmi->capt_xrun() * 1000.0, _pcmi->play_xrun() * 1000.0);
#endif
			}
		} else {
			// Freewheelin'

			// zero audio input buffers
			for (std::vector<AlsaPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
				memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
			}

			clock1 = g_get_monotonic_time();
			uint32_t i = 0;
			for (std::vector<AlsaPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
				static_cast<AlsaMidiBuffer*>((*it)->get_buffer(0))->clear ();
				AlsaMidiIn *rm = _rmidi_in.at(i);
				void *bptr = (*it)->get_buffer(0);
				midi_clear(bptr); // zero midi buffer

				// TODO add an API call for this.
				pframes_t time;
				uint8_t data[64]; // match MaxAlsaEventSize in alsa_rawmidi.cc
				size_t size = sizeof(data);
				while (rm->recv_event (time, data, size)) {
					; // discard midi-data from HW.
				}
				rm->sync_time (clock1);
			}

			_last_process_start = 0;
			if (engine.process_callback (_samples_per_period)) {
				_pcmi->pcm_stop ();
				_active = false;
				return 0;
			}

			// drop all outgoing MIDI messages
			for (std::vector<AlsaPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
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
	_pcmi->pcm_stop ();
	_active = false;
	if (_run) {
		engine.halted_callback("ALSA I/O error.");
	}
	return 0;
}


/******************************************************************************/

static boost::shared_ptr<AlsaAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	"ALSA",
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
		_instance.reset (new AlsaAudioBackend (e, _descriptor));
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
AlsaPort::AlsaPort (AlsaAudioBackend &b, const std::string& name, PortFlags flags)
	: _alsa_backend (b)
	, _name  (name)
	, _flags (flags)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
}

AlsaPort::~AlsaPort () {
	disconnect_all ();
}


int AlsaPort::connect (AlsaPort *port)
{
	if (!port) {
		PBD::error << _("AlsaPort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("AlsaPort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("AlsaPort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("AlsaPort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("AlsaPort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
#if 0 // don't bother to warn about this for now. just ignore it
		PBD::error << _("AlsaPort::connect (): ports are already connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
#endif
		return -1;
	}

	_connect (port, true);
	return 0;
}


void AlsaPort::_connect (AlsaPort *port, bool callback)
{
	_connections.push_back (port);
	if (callback) {
		port->_connect (this, false);
		_alsa_backend.port_connect_callback (name(),  port->name(), true);
	}
}

int AlsaPort::disconnect (AlsaPort *port)
{
	if (!port) {
		PBD::error << _("AlsaPort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("AlsaPort::disconnect (): ports are not connected:")
			<< " (" << name () << ") -> (" << port->name () << ")"
			<< endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void AlsaPort::_disconnect (AlsaPort *port, bool callback)
{
	std::vector<AlsaPort*>::iterator it = std::find (_connections.begin (), _connections.end (), port);

	assert (it != _connections.end ());

	_connections.erase (it);

	if (callback) {
		port->_disconnect (this, false);
		_alsa_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void AlsaPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_alsa_backend.port_connect_callback (name(),  _connections.back ()->name(), false);
		_connections.pop_back ();
	}
}

bool
AlsaPort::is_connected (const AlsaPort *port) const
{
	return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
}

bool AlsaPort::is_physically_connected () const
{
	for (std::vector<AlsaPort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

/******************************************************************************/

AlsaAudioPort::AlsaAudioPort (AlsaAudioBackend &b, const std::string& name, PortFlags flags)
	: AlsaPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock(_buffer, sizeof (_buffer));
}

AlsaAudioPort::~AlsaAudioPort () { }

void* AlsaAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<AlsaPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			AlsaAudioPort const * source = static_cast<const AlsaAudioPort*>(*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<const AlsaAudioPort*>(*it);
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


AlsaMidiPort::AlsaMidiPort (AlsaAudioBackend &b, const std::string& name, PortFlags flags)
	: AlsaPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();
}

AlsaMidiPort::~AlsaMidiPort () { }

struct MidiEventSorter {
	bool operator() (const boost::shared_ptr<AlsaMidiEvent>& a, const boost::shared_ptr<AlsaMidiEvent>& b) {
		return *a < *b;
	}
};

void* AlsaMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		for (std::vector<AlsaPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			const AlsaMidiBuffer * src = static_cast<const AlsaMidiPort*>(*i)->const_buffer ();
			for (AlsaMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (boost::shared_ptr<AlsaMidiEvent>(new AlsaMidiEvent (**it)));
			}
		}
		std::sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter());
	}
	return &(_buffer[_bufperiod]);
}

AlsaMidiEvent::AlsaMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
	, _data (0)
{
	if (size > 0) {
		_data = (uint8_t*) malloc (size);
		memcpy (_data, data, size);
	}
}

AlsaMidiEvent::AlsaMidiEvent (const AlsaMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
	, _data (0)
{
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

AlsaMidiEvent::~AlsaMidiEvent () {
	free (_data);
};
