/*
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2021 Robin Gareus <robin@gareus.org>
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

#include <glibmm.h>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "alsa_audiobackend.h"

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/port_manager.h"
#include "ardouralsautil/devicelist.h"
#include "pbd/i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;

size_t                                  AlsaAudioBackend::_max_buffer_size = 8192;
std::vector<std::string>                AlsaAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_input_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_output_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_duplex_audio_device_status;
std::vector<AudioBackend::DeviceStatus> AlsaAudioBackend::_midi_device_status;

ALSADeviceInfo AlsaAudioBackend::_input_audio_device_info;
ALSADeviceInfo AlsaAudioBackend::_output_audio_device_info;

AlsaAudioBackend::AlsaAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, PortEngineSharedImpl (e, s_instance_name)
	, _pcmi (0)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _freewheeling (false)
	, _measure_latency (false)
	, _last_process_start (0)
	, _input_audio_device ("")
	, _output_audio_device ("")
	, _midi_driver_option (get_standard_device_name (DeviceNone))
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _periods_per_cycle (2)
	, _n_inputs (0)
	, _n_outputs (0)
	, _systemic_audio_input_latency (0)
	, _systemic_audio_output_latency (0)
	, _midi_device_thread_active (false)
	, _dsp_load (0)
	, _processed_samples (0)
{
	_instance_name = s_instance_name;
	pthread_mutex_init (&_device_port_mutex, 0);
	_input_audio_device_info.valid  = false;
	_output_audio_device_info.valid = false;

	_port_connection_queue.reserve (128);
}

AlsaAudioBackend::~AlsaAudioBackend ()
{
	clear_ports ();

	pthread_mutex_destroy (&_device_port_mutex);
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
	_duplex_audio_device_status.clear ();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names (devices);
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (_input_audio_device == "") {
			_input_audio_device = i->first;
		}
		if (_output_audio_device == "") {
			_output_audio_device = i->first;
		}
		_duplex_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _duplex_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_input_devices () const
{
	_input_audio_device_status.clear ();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names (devices, HalfDuplexIn);
	_input_audio_device_status.push_back (DeviceStatus (get_standard_device_name (DeviceNone), true));
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (_input_audio_device == "") {
			_input_audio_device = i->first;
		}
		_input_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _input_audio_device_status;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_output_devices () const
{
	_output_audio_device_status.clear ();
	std::map<std::string, std::string> devices;
	get_alsa_audio_device_names (devices, HalfDuplexOut);
	_output_audio_device_status.push_back (DeviceStatus (get_standard_device_name (DeviceNone), true));
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (_output_audio_device == "") {
			_output_audio_device = i->first;
		}
		_output_audio_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _output_audio_device_status;
}

std::vector<float>
AlsaAudioBackend::available_sample_rates2 (const std::string& input_device, const std::string& output_device) const
{
	std::vector<float> sr;
	if (input_device == get_standard_device_name (DeviceNone) && output_device == get_standard_device_name (DeviceNone)) {
		return sr;
	} else if (input_device == get_standard_device_name (DeviceNone)) {
		sr = available_sample_rates (output_device);
	} else if (output_device == get_standard_device_name (DeviceNone)) {
		sr = available_sample_rates (input_device);
	} else {
		std::vector<float> sr_in  = available_sample_rates (input_device);
		std::vector<float> sr_out = available_sample_rates (output_device);
		std::set_intersection (sr_in.begin (), sr_in.end (), sr_out.begin (), sr_out.end (), std::back_inserter (sr));
	}
	return sr;
}

std::vector<float>
AlsaAudioBackend::available_sample_rates (const std::string& device) const
{
	ALSADeviceInfo*    nfo = NULL;
	std::vector<float> sr;
	if (device == get_standard_device_name (DeviceNone)) {
		return sr;
	}
	if (device == _input_audio_device && _input_audio_device_info.valid) {
		nfo = &_input_audio_device_info;
	} else if (device == _output_audio_device && _output_audio_device_info.valid) {
		nfo = &_output_audio_device_info;
	}

	static const float avail_rates[] = { 8000, 22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

	for (size_t i = 0; i < sizeof (avail_rates) / sizeof (float); ++i) {
		if (!nfo || (avail_rates[i] >= nfo->min_rate && avail_rates[i] <= nfo->max_rate)) {
			sr.push_back (avail_rates[i]);
		}
	}

	return sr;
}

std::vector<uint32_t>
AlsaAudioBackend::available_buffer_sizes2 (const std::string& input_device, const std::string& output_device) const
{
	std::vector<uint32_t> bs;
	if (input_device == get_standard_device_name (DeviceNone) && output_device == get_standard_device_name (DeviceNone)) {
		return bs;
	} else if (input_device == get_standard_device_name (DeviceNone)) {
		bs = available_buffer_sizes (output_device);
	} else if (output_device == get_standard_device_name (DeviceNone)) {
		bs = available_buffer_sizes (input_device);
	} else {
		std::vector<uint32_t> bs_in  = available_buffer_sizes (input_device);
		std::vector<uint32_t> bs_out = available_buffer_sizes (output_device);
		std::set_intersection (bs_in.begin (), bs_in.end (), bs_out.begin (), bs_out.end (), std::back_inserter (bs));
	}
	return bs;
}

std::vector<uint32_t>
AlsaAudioBackend::available_buffer_sizes (const std::string& device) const
{
	ALSADeviceInfo*       nfo = NULL;
	std::vector<uint32_t> bs;
	if (device == get_standard_device_name (DeviceNone)) {
		return bs;
	}
	if (device == _input_audio_device && _input_audio_device_info.valid) {
		nfo = &_input_audio_device_info;
	} else if (device == _output_audio_device && _output_audio_device_info.valid) {
		nfo = &_output_audio_device_info;
	}

	static const unsigned long avail_sizes[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

	for (size_t i = 0; i < sizeof (avail_sizes) / sizeof (unsigned long); ++i) {
		if (!nfo || (avail_sizes[i] >= nfo->min_size && avail_sizes[i] <= nfo->max_size)) {
			bs.push_back (avail_sizes[i]);
		}
	}

	if (!nfo) {
		return bs;
	}

	static const unsigned long try_msec[] = { 2, 4, 5, 6, 8, 10, 15, 20, 25, 40 };

	for (size_t i = 0; i < sizeof (try_msec) / sizeof (unsigned long); ++i) {
		unsigned int msbs = _samplerate * try_msec[i] / 1000;
		if (msbs >= nfo->min_size && msbs <= nfo->max_size) {
			bs.push_back (msbs);
		}
	}

	std::sort (bs.begin (), bs.end ());
	return bs;
}

uint32_t
AlsaAudioBackend::available_input_channel_count (const std::string& device) const
{
	if (device == get_standard_device_name (DeviceNone)) {
		return 0;
	}
	if (device == _input_audio_device && _input_audio_device_info.valid) {
		return _input_audio_device_info.max_channels;
	}
	return 128;
}

uint32_t
AlsaAudioBackend::available_output_channel_count (const std::string& device) const
{
	if (device == get_standard_device_name (DeviceNone)) {
		return 0;
	}
	if (device == _output_audio_device && _output_audio_device_info.valid) {
		return _output_audio_device_info.max_channels;
	}
	return 128;
}

std::vector<uint32_t>
AlsaAudioBackend::available_period_sizes (const std::string& driver, const std::string& device) const
{
	std::vector<uint32_t> ps;
	ps.push_back (2);

	ALSADeviceInfo* nfo = NULL;
	if (device == get_standard_device_name (DeviceNone)) {
		return ps;
	}

	if (device == _output_audio_device && _output_audio_device_info.valid) {
		nfo = &_output_audio_device_info;
		if (nfo->max_nper > 2) {
			ps.push_back (3);
		}
		if (nfo->min_nper > 3) {
			ps.push_back (nfo->min_nper);
		}
	} else {
		ps.push_back (3);
	}

	return ps;
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
	if (_input_audio_device == d && _input_audio_device_info.valid) {
		return 0;
	}
	_input_audio_device = d;

	if (d == get_standard_device_name (DeviceNone)) {
		_input_audio_device_info.valid = false;
		return 0;
	}
	std::string alsa_device;
	std::map<std::string, std::string> devices;

	get_alsa_audio_device_names (devices, HalfDuplexIn);
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (i->first == d) {
			alsa_device = i->second;
			break;
		}
	}
	if (alsa_device == "") {
		_input_audio_device_info.valid = false;
		return 1;
	}
	/* device will be busy once used, hence cache the parameters */
	/* return */ get_alsa_device_parameters (alsa_device.c_str (), false, &_input_audio_device_info);
	return 0;
}

int
AlsaAudioBackend::set_output_device_name (const std::string& d)
{
	if (_output_audio_device == d && _output_audio_device_info.valid) {
		return 0;
	}

	_output_audio_device = d;

	if (d == get_standard_device_name (DeviceNone)) {
		_output_audio_device_info.valid = false;
		return 0;
	}
	std::string alsa_device;
	std::map<std::string, std::string> devices;

	get_alsa_audio_device_names (devices, HalfDuplexOut);
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (i->first == d) {
			alsa_device = i->second;
			break;
		}
	}
	if (alsa_device == "") {
		_output_audio_device_info.valid = false;
		return 1;
	}
	/* return */ get_alsa_device_parameters (alsa_device.c_str (), true, &_output_audio_device_info);
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

bool
AlsaAudioBackend::can_measure_systemic_latency () const
{
	return _input_audio_device == _output_audio_device && _input_audio_device != get_standard_device_name (DeviceNone);
}

int
AlsaAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) {
		return -1;
	}
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
AlsaAudioBackend::set_peridod_size (uint32_t n)
{
	if (n == 0) {
		return -1;
	}
	if (_run) {
		return -1;
	}
	_periods_per_cycle = n;
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
	if (!yn) {
		return 0;
	}
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
	if (_systemic_audio_input_latency == sl) {
		return 0;
	}
	_systemic_audio_input_latency = sl;
	if (_run) {
		update_systemic_audio_latencies ();
	}
	return 0;
}

int
AlsaAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	if (_systemic_audio_output_latency == sl) {
		return 0;
	}
	_systemic_audio_output_latency = sl;
	if (_run) {
		update_systemic_audio_latencies ();
	}
	return 0;
}

int
AlsaAudioBackend::set_systemic_midi_input_latency (std::string const device, uint32_t sl)
{
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return -1;
	}
	nfo->systemic_input_latency = sl;
	if (_run && nfo->enabled) {
		update_systemic_midi_latencies ();
	}
	return 0;
}

int
AlsaAudioBackend::set_systemic_midi_output_latency (std::string const device, uint32_t sl)
{
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return -1;
	}
	nfo->systemic_output_latency = sl;
	if (_run && nfo->enabled) {
		update_systemic_midi_latencies ();
	}
	return 0;
}

void
AlsaAudioBackend::update_systemic_audio_latencies ()
{
	const uint32_t lcpp = (_periods_per_cycle - 2) * _samples_per_period;
	LatencyRange   lr;

	lr.min = lr.max = (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		set_latency_range (*it, true, lr);
	}

	lr.min = lr.max = lcpp + (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
		set_latency_range (*it, false, lr);
	}
	update_latencies ();
}

void
AlsaAudioBackend::update_systemic_midi_latencies ()
{
	pthread_mutex_lock (&_device_port_mutex);
	uint32_t i = 0;
	for (std::vector<BackendPortPtr>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
		assert (_rmidi_out.size () > i);
		AlsaMidiOut*               rm  = _rmidi_out.at (i);
		struct AlsaMidiDeviceInfo* nfo = midi_device_info (rm->name ());
		assert (nfo);
		LatencyRange lr;
		lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_output_latency);
		set_latency_range (*it, true, lr);
	}

	i = 0;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
		assert (_rmidi_in.size () > i);
		AlsaMidiIO*                rm  = _rmidi_in.at (i);
		struct AlsaMidiDeviceInfo* nfo = midi_device_info (rm->name ());
		assert (nfo);
		LatencyRange lr;
		lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_input_latency);
		set_latency_range (*it, false, lr);
	}
	pthread_mutex_unlock (&_device_port_mutex);
	update_latencies ();
}

/* Retrieving parameters */
std::string
AlsaAudioBackend::device_name () const
{
	if (_input_audio_device != get_standard_device_name (DeviceNone)) {
		return _input_audio_device;
	}
	if (_output_audio_device != get_standard_device_name (DeviceNone)) {
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

uint32_t
AlsaAudioBackend::period_size () const
{
	return _periods_per_cycle;
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
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return 0;
	}
	return nfo->systemic_input_latency;
}

uint32_t
AlsaAudioBackend::systemic_midi_output_latency (std::string const device) const
{
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return 0;
	}
	return nfo->systemic_output_latency;
}

/* MIDI */
struct AlsaAudioBackend::AlsaMidiDeviceInfo*
AlsaAudioBackend::midi_device_info (std::string const name) const
{
	for (std::map<std::string, struct AlsaMidiDeviceInfo*>::const_iterator i = _midi_devices.begin (); i != _midi_devices.end (); ++i) {
		if (i->first == name) {
			return (i->second);
		}
	}

	assert (_midi_driver_option != get_standard_device_name (DeviceNone));

	std::map<std::string, std::string> devices;
	if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names (devices);
	} else {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (i->first == name) {
			_midi_devices[name] = new AlsaMidiDeviceInfo ();
			return _midi_devices[name];
		}
	}
	return 0;
}

std::vector<std::string>
AlsaAudioBackend::enumerate_midi_options () const
{
	if (_midi_options.empty ()) {
		_midi_options.push_back (_("ALSA raw devices"));
		_midi_options.push_back (_("ALSA sequencer"));
		_midi_options.push_back (get_standard_device_name (DeviceNone));
	}
	return _midi_options;
}

std::vector<AudioBackend::DeviceStatus>
AlsaAudioBackend::enumerate_midi_devices () const
{
	_midi_device_status.clear ();
	std::map<std::string, std::string> devices;

	if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names (devices);
	} else if (_midi_driver_option == _("ALSA sequencer")) {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		_midi_device_status.push_back (DeviceStatus (i->first, true));
	}
	return _midi_device_status;
}

int
AlsaAudioBackend::set_midi_option (const std::string& opt)
{
	if (opt != get_standard_device_name (DeviceNone) && opt != _("ALSA raw devices") && opt != _("ALSA sequencer")) {
		return -1;
	}
	if (_run && _midi_driver_option != opt) {
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
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return -1;
	}
	const bool prev_enabled = nfo->enabled;
	nfo->enabled            = enable;

	if (_run && prev_enabled != enable) {
		if (enable) {
			// add ports for the given device
			register_system_midi_ports (device);
		} else {
			// remove all ports provided by the given device
			pthread_mutex_lock (&_device_port_mutex);
			uint32_t i = 0;
			for (std::vector<BackendPortPtr>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end ();) {
				assert (_rmidi_out.size () > i);
				AlsaMidiOut* rm = _rmidi_out.at (i);
				if (rm->name () != device) {
					++it;
					++i;
					continue;
				}
				unregister_port (*it);
				it = _system_midi_out.erase (it);
				rm->stop ();
				assert (rm == *(_rmidi_out.begin () + i));
				_rmidi_out.erase (_rmidi_out.begin () + i);
				delete rm;
			}

			i = 0;
			for (std::vector<BackendPortPtr>::iterator it = _system_midi_in.begin (); it != _system_midi_in.end ();) {
				assert (_rmidi_in.size () > i);
				AlsaMidiIn* rm = _rmidi_in.at (i);
				if (rm->name () != device) {
					++it;
					++i;
					continue;
				}
				unregister_port (*it);
				it = _system_midi_in.erase (it);
				rm->stop ();
				assert (rm == *(_rmidi_in.begin () + i));
				_rmidi_in.erase (_rmidi_in.begin () + i);
				delete rm;
			}
			pthread_mutex_unlock (&_device_port_mutex);
		}
		update_systemic_midi_latencies ();
	}
	return 0;
}

bool
AlsaAudioBackend::midi_device_enabled (std::string const device) const
{
	struct AlsaMidiDeviceInfo* nfo = midi_device_info (device);
	if (!nfo) {
		return false;
	}
	return nfo->enabled;
}

/* State Control */

static void*
pthread_process (void* arg)
{
	AlsaAudioBackend* d = static_cast<AlsaAudioBackend*> (arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
AlsaAudioBackend::_start (bool for_latency_measurement)
{
	if (!_active && _run) {
		// recover from 'halted', reap threads
		stop ();
	}

	if (_active || _run) {
		if (for_latency_measurement != _measure_latency) {
			_measure_latency = for_latency_measurement;
			update_systemic_audio_latencies ();
			update_systemic_midi_latencies ();
			PBD::info << _("AlsaAudioBackend: reload latencies.") << endmsg;
			return NoError;
		}
		PBD::info << _("AlsaAudioBackend: already active.") << endmsg;
		return BackendReinitializationError;
	}

	_measure_latency = for_latency_measurement;

	clear_ports ();

	/* reset internal state */
	_dsp_load           = 0;
	_freewheeling       = false;
	_freewheel          = false;
	_last_process_start = 0;

	_device_reservation.release_device ();

	assert (_rmidi_in.size () == 0);
	assert (_rmidi_out.size () == 0);
	assert (_pcmi == 0);

	int         duplex = 0;
	std::string audio_device;
	std::string alsa_device;

	std::map<std::string, std::string> devices;

	if (_input_audio_device == get_standard_device_name (DeviceNone) && _output_audio_device == get_standard_device_name (DeviceNone)) {
		PBD::error << _("AlsaAudioBackend: At least one of input or output device needs to be set.");
		return AudioDeviceInvalidError;
	}

	std::string            slave_device;
	AudioSlave::DuplexMode slave_duplex = AudioSlave::FullDuplex;

	if (_input_audio_device != _output_audio_device) {
		if (_input_audio_device != get_standard_device_name (DeviceNone) && _output_audio_device != get_standard_device_name (DeviceNone)) {
			/* Different devices for In + Out.
			 * Ideally use input as clock source, and resample output.
			 * But when using separate devices, input is usually one (or more)
			 * cheap USB mic. Also keeping output device as "main",
			 * retains master-out connection.
			 */
			if (getenv ("ARDOUR_ALSA_CLK")) {
				slave_device         = _output_audio_device;
				_output_audio_device = get_standard_device_name (DeviceNone);
				slave_duplex         = AudioSlave::HalfDuplexOut;
			} else {
				slave_device        = _input_audio_device;
				_input_audio_device = get_standard_device_name (DeviceNone);
				slave_duplex        = AudioSlave::HalfDuplexIn;
			}
		}
		if (_input_audio_device != get_standard_device_name (DeviceNone)) {
			get_alsa_audio_device_names (devices, HalfDuplexIn);
			audio_device = _input_audio_device;
			duplex       = 1;
		} else {
			get_alsa_audio_device_names (devices, HalfDuplexOut);
			audio_device = _output_audio_device;
			duplex       = 2;
		}
	} else {
		get_alsa_audio_device_names (devices);
		audio_device = _input_audio_device;
		duplex       = 3;
	}

	std::map<std::string, std::string>::const_iterator di = devices.find (audio_device);

	if (di == devices.end ()) {
		PBD::error << _("AlsaAudioBackend: Cannot find configured device. Is it still connected?");
		return AudioDeviceNotAvailableError;
	} else {
		alsa_device = di->second;
		assert (!alsa_device.empty ());
	}

	_device_reservation.acquire_device (alsa_device.c_str ());
	_pcmi = new Alsa_pcmi (
	    (duplex & 2) ? alsa_device.c_str () : NULL,
	    (duplex & 1) ? alsa_device.c_str () : NULL,
	    /* ctrl name */ 0,
	    _samplerate, _samples_per_period,
	    _periods_per_cycle, _periods_per_cycle,
	    /* debug */ 0);

	AudioBackend::ErrorCode error_code = NoError;
	switch (_pcmi->state ()) {
		case 0: /* OK */
			break;
		case -1:
			PBD::error << _("AlsaAudioBackend: failed to open device.") << endmsg;
			error_code = AudioDeviceOpenError;
			break;
		case -2:
			PBD::error << _("AlsaAudioBackend: failed to allocate parameters.") << endmsg;
			error_code = AudioDeviceOpenError;
			break;
		case -3:
			PBD::error << _("AlsaAudioBackend: cannot set requested sample rate.")
			           << endmsg;
			error_code = SampleRateNotSupportedError;
			break;
		case -4:
			PBD::error << _("AlsaAudioBackend: cannot set requested period size.")
			           << endmsg;
			error_code = PeriodSizeNotSupportedError;
			break;
		case -5:
			PBD::error << _("AlsaAudioBackend: cannot set requested number of periods.")
			           << endmsg;
			error_code = PeriodCountNotSupportedError;
			break;
		case -6:
			PBD::error << _("AlsaAudioBackend: unsupported sample format.") << endmsg;
			error_code = SampleFormatNotSupportedError;
			break;
		default:
			PBD::error << _("AlsaAudioBackend: initialization failed.") << endmsg;
			error_code = AudioDeviceOpenError;
			break;
	}

	if (_pcmi->state ()) {
		delete _pcmi;
		_pcmi = 0;
		_device_reservation.release_device ();
		return error_code;
	}

#ifndef NDEBUG
	fprintf (stdout, " --[[ ALSA Device %s\n", alsa_device.c_str ());
	_pcmi->printinfo ();
	fprintf (stdout, " --]]\n");
#else
	/* If any debug parameter is set, print info */
	if (getenv ("ARDOUR_ALSA_DEBUG")) {
		fprintf (stdout, " --[[ ALSA Device %s\n", alsa_device.c_str ());
		_pcmi->printinfo ();
		fprintf (stdout, " --]]\n");
	}
#endif

	if (_n_outputs != _pcmi->nplay ()) {
		if (_n_outputs == 0) {
			_n_outputs = _pcmi->nplay ();
		} else {
			_n_outputs = std::min (_n_outputs, _pcmi->nplay ());
		}
		PBD::info << _("AlsaAudioBackend: adjusted output channel count to match device.") << endmsg;
	}

	if (_n_inputs != _pcmi->ncapt ()) {
		if (_n_inputs == 0) {
			_n_inputs = _pcmi->ncapt ();
		} else {
			_n_inputs = std::min (_n_inputs, _pcmi->ncapt ());
		}
		PBD::info << _("AlsaAudioBackend: adjusted input channel count to match device.") << endmsg;
	}

	if (_pcmi->fsize () != _samples_per_period) {
		_samples_per_period = _pcmi->fsize ();
		PBD::warning << string_compose (_("AlsaAudioBackend: samples per period does not match, using %1."), _samples_per_period) << endmsg;
	}

	if (_pcmi->fsamp () != _samplerate) {
		_samplerate = _pcmi->fsamp ();
		engine.sample_rate_change (_samplerate);
		PBD::warning << _("AlsaAudioBackend: sample rate does not match.") << endmsg;
	}

	register_system_midi_ports ();

	if (register_system_audio_ports ()) {
		PBD::error << _("AlsaAudioBackend: failed to register system ports.") << endmsg;
		delete _pcmi;
		_pcmi = 0;
		_device_reservation.release_device ();
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("AlsaAudioBackend: Could not re-establish ports.") << endmsg;
		delete _pcmi;
		_pcmi = 0;
		_device_reservation.release_device ();
		return PortReconnectError;
	}

	_run = true;
	g_atomic_int_set (&_port_change_flag, 0);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MAIN, PBD_RT_STACKSIZE_PROC,
	                                 &_main_thread, pthread_process, this)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &_main_thread, pthread_process, this)) {
			PBD::error << _("AlsaAudioBackend: failed to create process thread.") << endmsg;
			delete _pcmi;
			_pcmi = 0;
			_device_reservation.release_device ();
			_run = false;
			return ProcessThreadStartError;
		} else {
			PBD::warning << _("AlsaAudioBackend: cannot acquire realtime permissions.") << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) {
		Glib::usleep (1000);
	}

	if (timeout == 0 || !_active) {
		PBD::error << _("AlsaAudioBackend: failed to start process thread.") << endmsg;
		delete _pcmi;
		_pcmi = 0;
		_device_reservation.release_device ();
		_run = false;
		return ProcessThreadStartError;
	}

	_midi_device_thread_active = listen_for_midi_device_changes ();

	get_alsa_audio_device_names (devices, (AlsaDuplex)slave_duplex);

	if (!slave_device.empty () && (di = devices.find (slave_device)) != devices.end ()) {
		std::string dev = di->second;
		if (add_slave (dev.c_str (), _samplerate, _samples_per_period, _periods_per_cycle, slave_duplex)) {
			PBD::info << string_compose (_("ALSA slave '%1' added"), dev) << endmsg;
		} else {
			PBD::error << string_compose (_("ALSA failed to add '%1' as slave"), dev) << endmsg;
		}
	}

#if 1 // TODO: we need a GUI (and API) for this
	/* example: ARDOUR_ALSA_EXT="hw:2@48000/512*3;hw:3@44100" */
	if (NULL != getenv ("ARDOUR_ALSA_EXT")) {
		boost::char_separator<char> sep (";");
		std::string                 ext (getenv ("ARDOUR_ALSA_EXT"));

		boost::tokenizer<boost::char_separator<char> > devs (ext, sep);

		BOOST_FOREACH (const std::string& tmp, devs) {
			std::string            dev (tmp);
			unsigned int           sr     = _samplerate;
			unsigned int           spp    = _samples_per_period;
			unsigned int           ppc    = _periods_per_cycle;
			AudioSlave::DuplexMode duplex = AudioSlave::FullDuplex;
			std::string::size_type n      = dev.find ('@');

			if (n != std::string::npos) {
				std::string const opt (dev.substr (n + 1));
				sr  = PBD::atoi (opt);
				dev = dev.substr (0, n);

				std::string::size_type n = opt.find ('/');
				if (n != std::string::npos) {
					std::string const opt2 (opt.substr (n + 1));
					spp = PBD::atoi (opt2);

					std::string::size_type n = opt2.find ('*');
					if (n != std::string::npos) {
						ppc = PBD::atoi (opt2.substr (n + 1));
					}
				}
			}
			if (add_slave (dev.c_str (), sr, spp, ppc, duplex)) {
				PBD::info << string_compose (_("ALSA slave '%1' added"), dev) << endmsg;
			} else {
				PBD::error << string_compose (_("ALSA failed to add '%1' as slave"), dev) << endmsg;
			}
		}
	}
#endif

	engine.reconnect_ports ();

	return NoError;
}

int
AlsaAudioBackend::stop ()
{
	void* status;
	if (!_run) {
		return 0;
	}

	_run = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("AlsaAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}

	stop_listen_for_midi_device_changes ();

	while (!_rmidi_out.empty ()) {
		AlsaMidiIO* m = _rmidi_out.back ();
		m->stop ();
		_rmidi_out.pop_back ();
		delete m;
	}
	while (!_rmidi_in.empty ()) {
		AlsaMidiIO* m = _rmidi_in.back ();
		m->stop ();
		_rmidi_in.pop_back ();
		delete m;
	}

	while (!_slaves.empty ()) {
		AudioSlave* s = _slaves.back ();
		_slaves.pop_back ();
		delete s;
	}

	unregister_ports ();
	delete _pcmi;
	_pcmi = 0;
	_device_reservation.release_device ();
	_measure_latency = false;

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
	return 100.f * _dsp_load;
}

size_t
AlsaAudioBackend::raw_buffer_size (DataType t)
{
	switch (t) {
		case DataType::AUDIO:
			return _samples_per_period * sizeof (Sample);
		case DataType::MIDI:
			return _max_buffer_size; // XXX not really limited
	}
	return 0;
}

/* Process time */
samplepos_t
AlsaAudioBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
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

	const int64_t elapsed_time_us = g_get_monotonic_time () - _last_process_start;
	return std::max ((pframes_t)0, (pframes_t)rint (1e-6 * elapsed_time_us * _samplerate));
}

void*
AlsaAudioBackend::alsa_process_thread (void* arg)
{
	ThreadData* td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f = td->f;
	delete td;
	f ();
	return 0;
}

int
AlsaAudioBackend::create_process_thread (boost::function<void ()> func)
{
	pthread_t   thread_id;
	ThreadData* td = new ThreadData (this, func, PBD_RT_STACKSIZE_PROC);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_PROC, PBD_RT_STACKSIZE_PROC, &thread_id, alsa_process_thread, td)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &thread_id, alsa_process_thread, td)) {
			PBD::error << _("AudioEngine: cannot create process thread.") << endmsg;
			return -1;
		}
	}

	_threads.push_back (thread_id);
	return 0;
}

int
AlsaAudioBackend::join_process_threads ()
{
	int rv = 0;

	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i) {
		void* status;
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
	if (pthread_equal (_main_thread, pthread_self ()) != 0) {
		return true;
	}

	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i) {
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
	port_connect_add_remove_callback ();
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

int
AlsaAudioBackend::register_system_audio_ports ()
{
	LatencyRange lr;

	const int a_ins = _n_inputs;
	const int a_out = _n_outputs;

	const uint32_t lcpp = (_periods_per_cycle - 2) * _samples_per_period;

	/* audio ports */
	lr.min = lr.max = (_measure_latency ? 0 : _systemic_audio_input_latency);
	for (int i = 1; i <= a_ins; ++i) {
		char tmp[64];
		snprintf (tmp, sizeof (tmp), "system:capture_%d", i);
		PortHandle p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsOutput | IsPhysical | IsTerminal));
		if (!p)
			return -1;
		set_latency_range (p, false, lr);
		BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
		ap->set_hw_port_name (string_compose (_("Main In %1"), i));
		_system_inputs.push_back (ap);
	}

	lr.min = lr.max = lcpp + (_measure_latency ? 0 : _systemic_audio_output_latency);
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf (tmp, sizeof (tmp), "system:playback_%d", i);
		PortHandle p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsInput | IsPhysical | IsTerminal));
		if (!p)
			return -1;
		set_latency_range (p, true, lr);
		BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
		if (a_out == 2) {
			ap->set_hw_port_name (i == 1 ? _("Out Left") : _("Out Right"));
		} else {
			ap->set_hw_port_name (string_compose (_("Main Out %1"), i));
		}
		_system_outputs.push_back (ap);
	}
	return 0;
}

void
AlsaAudioBackend::auto_update_midi_devices ()
{
	std::map<std::string, std::string> devices;
	if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names (devices);
	} else if (_midi_driver_option == _("ALSA sequencer")) {
		get_alsa_sequencer_names (devices);
	} else {
		return;
	}

	/* find new devices */
	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (_midi_devices.find (i->first) != _midi_devices.end ()) {
			continue;
		}
		_midi_devices[i->first] = new AlsaMidiDeviceInfo (false);
		set_midi_device_enabled (i->first, true);
	}

	for (std::map<std::string, struct AlsaMidiDeviceInfo*>::iterator i = _midi_devices.begin (); i != _midi_devices.end ();) {
		if (devices.find (i->first) != devices.end ()) {
			++i;
			continue;
		}
		set_midi_device_enabled (i->first, false);
		std::map<std::string, struct AlsaMidiDeviceInfo*>::iterator tmp = i;
		++tmp;
		_midi_devices.erase (i);
		i = tmp;
	}
}

void*
AlsaAudioBackend::_midi_device_thread (void* arg)
{
	AlsaAudioBackend* self = static_cast<AlsaAudioBackend*> (arg);
	pthread_set_name ("ALSA-MIDI-LIST");
	self->midi_device_thread ();
	pthread_exit (0);
	return 0;
}

void
AlsaAudioBackend::midi_device_thread ()
{
	snd_seq_t* seq;
	if (snd_seq_open (&seq, "hw", SND_SEQ_OPEN_INPUT, 0) < 0) {
		return;
	}
	if (snd_seq_set_client_name (seq, "Ardour")) {
		snd_seq_close (seq);
		return;
	}
	if (snd_seq_nonblock (seq, 1) < 0) {
		snd_seq_close (seq);
		return;
	}

	int npfds = snd_seq_poll_descriptors_count (seq, POLLIN);
	if (npfds < 1) {
		snd_seq_close (seq);
		return;
	}

	int port = snd_seq_create_simple_port (seq, "port", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_NO_EXPORT, SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_connect_from (seq, port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);

	struct pollfd* pfds = (struct pollfd*)malloc (npfds * sizeof (struct pollfd));
	snd_seq_poll_descriptors (seq, pfds, npfds, POLLIN);
	snd_seq_drop_input (seq);

	bool do_poll = true;
	while (_run) {
		if (do_poll) {
			int perr = poll (pfds, npfds, 200 /* ms */);
			if (perr == 0) {
				continue;
			}
			if (perr < 0) {
				break;
			}
		}

		snd_seq_event_t* event;
		ssize_t          err = snd_seq_event_input (seq, &event);
#if EAGAIN == EWOULDBLOCK
		if ((err == -EAGAIN) || (err == -ENOSPC))
#else
		if ((err == -EAGAIN) || (err == -EWOULDBLOCK) || (err == -ENOSPC))
#endif
		{
			do_poll = true;
			continue;
		}
		if (err < 0) {
			break;
		}

		assert (event->source.client == SND_SEQ_CLIENT_SYSTEM);

		switch (event->type) {
			case SND_SEQ_EVENT_PORT_START:
			case SND_SEQ_EVENT_PORT_EXIT:
			case SND_SEQ_EVENT_PORT_CHANGE:
				auto_update_midi_devices ();
				engine.request_device_list_update ();
			default:
				break;
		}
		do_poll = (0 == err);
	}
	free (pfds);
	snd_seq_delete_simple_port (seq, port);
	snd_seq_close (seq);
}

bool
AlsaAudioBackend::listen_for_midi_device_changes ()
{
	if (pthread_create (&_midi_device_thread_id, NULL, _midi_device_thread, this)) {
		return false;
	}
	return true;
}

void
AlsaAudioBackend::stop_listen_for_midi_device_changes ()
{
	if (!_midi_device_thread_active) {
		return;
	}
	pthread_join (_midi_device_thread_id, NULL);
	_midi_device_thread_active = false;
}

/* set playback-latency for _system_inputs
 * and capture-latency for _system_outputs
 */
void
AlsaAudioBackend::update_system_port_latencies ()
{
	pthread_mutex_lock (&_device_port_mutex);

	PortEngineSharedImpl::update_system_port_latencies ();

	pthread_mutex_unlock (&_device_port_mutex);

	for (AudioSlaves::iterator s = _slaves.begin (); s != _slaves.end (); ++s) {
		if ((*s)->dead) {
			continue;
		}

		for (std::vector<BackendPortPtr>::const_iterator it = (*s)->inputs.begin (); it != (*s)->inputs.end (); ++it) {
			(*it)->update_connected_latency (true);
		}

		for (std::vector<BackendPortPtr>::const_iterator it = (*s)->outputs.begin (); it != (*s)->outputs.end (); ++it) {
			(*it)->update_connected_latency (false);
		}
	}
}

/* libs/ardouralsautil/devicelist.cc appends either of
 * " (IO)", " (I)", or " (O)"
 * depending of the device is full-duples or half-duplex
 */
static std::string
replace_name_io (std::string const& name, bool in)
{
	if (name.empty ()) {
		return "";
	}
	size_t pos = name.find_last_of ('(');
	if (pos == std::string::npos) {
		assert (0); // this should never happen.
		return name;
	}
	return name.substr (0, pos) + "(" + (in ? "In" : "Out") + ")";
}

static uint32_t
elf_hash (std::string const& s)
{
	const uint8_t* b = (const uint8_t*)s.c_str ();
	uint32_t       h = 0;
	for (size_t i = 0; i < s.length (); ++i) {
		h             = (h << 4) + b[i];
		uint32_t high = h & 0xF0000000;
		if (high) {
			h ^= high >> 24;
			h &= ~high;
		}
	}
	return h;
}

int
AlsaAudioBackend::register_system_midi_ports (const std::string device)
{
	std::map<std::string, std::string> devices;

	if (_midi_driver_option == get_standard_device_name (DeviceNone)) {
		return 0;
	} else if (_midi_driver_option == _("ALSA raw devices")) {
		get_alsa_rawmidi_device_names (devices);
	} else {
		get_alsa_sequencer_names (devices);
	}

	for (std::map<std::string, std::string>::const_iterator i = devices.begin (); i != devices.end (); ++i) {
		if (!device.empty () && device != i->first) {
			continue;
		}
		struct AlsaMidiDeviceInfo* nfo = midi_device_info (i->first);
		if (!nfo) {
			continue;
		}
		if (!nfo->enabled) {
			continue;
		}

		AlsaMidiOut* mout;
		if (_midi_driver_option == _("ALSA raw devices")) {
			mout = new AlsaRawMidiOut (i->first, i->second.c_str ());
		} else {
			mout = new AlsaSeqMidiOut (i->first, i->second.c_str ());
		}

		if (mout->state ()) {
			PBD::warning << string_compose (_("AlsaMidiOut: failed to open midi device '%1'."), i->second) << endmsg;
			delete mout;
		} else {
			mout->setup_timing (_samples_per_period, _samplerate);
			mout->sync_time (g_get_monotonic_time ());
			if (mout->start ()) {
				PBD::warning << string_compose (_("AlsaMidiOut: failed to start midi device '%1'."), i->second) << endmsg;
				delete mout;
			} else {
				char tmp[64];
				for (int x = 0; x < 10; ++x) {
					snprintf (tmp, sizeof (tmp), "system:midi_playback_%x%d", elf_hash (i->first), x);
					if (!find_port (tmp)) {
						break;
					}
				}
				PortHandle p = add_port (std::string (tmp), DataType::MIDI, static_cast<PortFlags> (IsInput | IsPhysical | IsTerminal));
				if (!p) {
					mout->stop ();
					delete mout;
				} else {
					LatencyRange lr;
					lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_output_latency);
					set_latency_range (p, true, lr);
					boost::dynamic_pointer_cast<AlsaMidiPort> (p)->set_n_periods (_periods_per_cycle); // TODO check MIDI alignment
					BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
					ap->set_hw_port_name (replace_name_io (i->first, false));
					pthread_mutex_lock (&_device_port_mutex);
					_system_midi_out.push_back (ap);
					pthread_mutex_unlock (&_device_port_mutex);
					_rmidi_out.push_back (mout);
				}
			}
		}

		AlsaMidiIn* midin;
		if (_midi_driver_option == _("ALSA raw devices")) {
			midin = new AlsaRawMidiIn (i->first, i->second.c_str ());
		} else {
			midin = new AlsaSeqMidiIn (i->first, i->second.c_str ());
		}

		if (midin->state ()) {
			PBD::warning << string_compose (_("AlsaMidiIn: failed to open midi device '%1'."), i->second) << endmsg;
			delete midin;
		} else {
			midin->setup_timing (_samples_per_period, _samplerate);
			midin->sync_time (g_get_monotonic_time ());
			if (midin->start ()) {
				PBD::warning << string_compose (_("AlsaMidiIn: failed to start midi device '%1'."), i->second) << endmsg;
				delete midin;
			} else {
				char tmp[64];
				for (int x = 0; x < 10; ++x) {
					snprintf (tmp, sizeof (tmp), "system:midi_capture_%x%d", elf_hash (i->first), x);
					if (!find_port (tmp)) {
						break;
					}
				}
				PortHandle p = add_port (std::string (tmp), DataType::MIDI, static_cast<PortFlags> (IsOutput | IsPhysical | IsTerminal));
				if (!p) {
					midin->stop ();
					delete midin;
					continue;
				}
				LatencyRange lr;
				lr.min = lr.max = (_measure_latency ? 0 : nfo->systemic_input_latency);
				set_latency_range (p, false, lr);
				BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
				ap->set_hw_port_name (replace_name_io (i->first, true));
				pthread_mutex_lock (&_device_port_mutex);
				_system_midi_in.push_back (ap);
				pthread_mutex_unlock (&_device_port_mutex);
				_rmidi_in.push_back (midin);
			}
		}
	}
	return 0;
}

/* MIDI */
int
AlsaAudioBackend::midi_event_get (
    pframes_t& timestamp,
    size_t& size, uint8_t const** buf, void* port_buffer,
    uint32_t event_index)
{
	assert (buf && port_buffer);
	AlsaMidiBuffer& source = *static_cast<AlsaMidiBuffer*> (port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	AlsaMidiEvent const& event = source[event_index];

	timestamp = event.timestamp ();
	size      = event.size ();
	*buf      = event.data ();
	return 0;
}

int
AlsaAudioBackend::midi_event_put (
    void*          port_buffer,
    pframes_t      timestamp,
    const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	if (size >= MaxAlsaMidiEventSize) {
		return -1;
	}
	AlsaMidiBuffer& dst = *static_cast<AlsaMidiBuffer*> (port_buffer);
#ifndef NDEBUG
	if (dst.size () && (pframes_t)dst.back ().timestamp () > timestamp) {
		// nevermind, ::get_buffer() sorts events
		fprintf (stderr, "AlsaMidiBuffer: it's too late for this event. %d > %d\n",
		         (pframes_t)dst.back ().timestamp (), timestamp);
	}
#endif
	dst.push_back (AlsaMidiEvent (timestamp, buffer, size));
	return 0;
}

uint32_t
AlsaAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<AlsaMidiBuffer*> (port_buffer)->size ();
}

void
AlsaAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
	AlsaMidiBuffer* buf = static_cast<AlsaMidiBuffer*> (port_buffer);
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
AlsaAudioBackend::set_latency_range (PortEngine::PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	if (!valid_port (port)) {
		PBD::error << _("AlsaPort::set_latency_range (): invalid port.") << endmsg;
	}
	port->set_latency_range (latency_range, for_playback);
}

LatencyRange
AlsaAudioBackend::get_latency_range (PortEngine::PortHandle port_handle, bool for_playback)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	LatencyRange   r;

	if (!valid_port (port)) {
		PBD::error << _("AlsaPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}

	r = port->latency_range (for_playback);
	if (port->is_physical () && port->is_terminal ()) {
		if (port->is_input () && for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
		if (port->is_output () && !for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
	}

	return r;
}

BackendPort*
AlsaAudioBackend::port_factory (std::string const& name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	BackendPort* port = 0;

	switch (type) {
		case DataType::AUDIO:
			port = new AlsaAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new AlsaMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << string_compose (_("%1::register_port: Invalid Data Type."), _instance_name) << endmsg;
			return 0;
	}

	return port;
}

/* Getting access to the data buffer for a port */

void*
AlsaAudioBackend::get_buffer (PortEngine::PortHandle port_handle, pframes_t nframes)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	assert (port);
	assert (valid_port (port));
	return port->get_buffer (nframes);
}

/* Engine Process */
void*
AlsaAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	bool reset_dll      = true;
	int  last_n_periods = 0;
	_active             = true;
	_processed_samples  = 0;

	double dll_dt = (double)_samples_per_period / (double)_samplerate;
	double dll_w1 = 2 * M_PI * 0.1 * dll_dt;
	double dll_w2 = dll_w1 * dll_w1;

	uint64_t  clock1;
	int       no_proc_errors = 0;
	const int bailout        = 5 * _samplerate / _samples_per_period;

	manager.registration_callback ();
	manager.graph_order_callback ();

	const double sr_norm = 1e-6 * (double)_samplerate / (double)_samples_per_period;

	/* warm up freewheel dry-run - see also AudioEngine _init_countdown */
	int cnt = std::max (4, (int)(_samplerate / _samples_per_period) / 8);
	for (int w = 0; w < cnt; ++w) {
		for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
		}
		if (engine.process_callback (_samples_per_period)) {
			_active = false;
			return 0;
		}
		Glib::usleep (1000000 * (_samples_per_period / _samplerate));
	}

	_dsp_load_calc.reset ();
	_pcmi->pcm_start ();

	while (_run) {
		long nr;
		bool xrun         = false;
		bool drain_slaves = false;

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
			for (AudioSlaves::iterator s = _slaves.begin (); s != _slaves.end (); ++s) {
				(*s)->freewheel (_freewheel);
			}
			if (!_freewheel) {
				_pcmi->pcm_stop ();
				_pcmi->pcm_start ();
				drain_slaves = true;
				_dsp_load_calc.reset ();
			}
		}

		if (!_freewheel) {
			dsp_stats[DeviceWait].start();
			nr = _pcmi->pcm_wait ();
			dsp_stats[DeviceWait].update();
			dsp_stats[RunLoop].start ();

			/* update DLL */
			uint64_t clock0 = g_get_monotonic_time ();
			if (reset_dll || last_n_periods != 1) {
				reset_dll    = false;
				drain_slaves = true;
				dll_dt       = 1e6 * (double)_samples_per_period / (double)_samplerate;
				_t0          = clock0;
				_t1          = clock0 + dll_dt;
			} else {
				const double er = clock0 - _t1;
				_t0 = _t1;
				_t1 = _t1 + dll_w1 * er + dll_dt;
				dll_dt += dll_w2 * er;
			}

			for (AudioSlaves::iterator s = _slaves.begin (); s != _slaves.end (); ++s) {
				if ((*s)->dead) {
					continue;
				}
				if ((*s)->halt) {
					/* slave died, unregister its ports (not rt-safe, but no matter) */
					PBD::error << _("ALSA Slave device halted") << endmsg;
					for (std::vector<BackendPortPtr>::const_iterator it = (*s)->inputs.begin (); it != (*s)->inputs.end (); ++it) {
						unregister_port (*it);
					}
					for (std::vector<BackendPortPtr>::const_iterator it = (*s)->outputs.begin (); it != (*s)->outputs.end (); ++it) {
						unregister_port (*it);
					}
					(*s)->inputs.clear ();
					(*s)->outputs.clear ();
					(*s)->active = false;
					(*s)->dead   = true;
					continue;
				}
				(*s)->active = (*s)->running () && (*s)->state () >= 0;
				if (!(*s)->active) {
					continue;
				}
				(*s)->cycle_start (_t0, (_t1 - _t0) * sr_norm, drain_slaves);
			}

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
				    << string_compose (_("AlsaAudioBackend: Audio Process Terminated after %1 consecutive xruns."), no_proc_errors) << endmsg;
				break;
			}

			last_n_periods = 0;
			while (nr >= (long)_samples_per_period && _freewheeling == _freewheel) {
				uint32_t i     = 0;
				clock1         = g_get_monotonic_time ();
				no_proc_errors = 0;

				_pcmi->capt_init (_samples_per_period);
				for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++i) {
					_pcmi->capt_chan (i, (float*)(*it)->get_buffer (_samples_per_period), _samples_per_period);
				}
				_pcmi->capt_done (_samples_per_period);

				for (AudioSlaves::iterator s = _slaves.begin (); s != _slaves.end (); ++s) {
					if (!(*s)->active) {
						continue;
					}
					i = 0;
					for (std::vector<BackendPortPtr>::const_iterator it = (*s)->inputs.begin (); it != (*s)->inputs.end (); ++it, ++i) {
						(*s)->capt_chan (i, (float*)(boost::dynamic_pointer_cast<BackendPort> (*it)->get_buffer (_samples_per_period)), _samples_per_period);
					}
				}

				/* only used when adding/removing MIDI device/system ports */
				pthread_mutex_lock (&_device_port_mutex);
				/* de-queue incoming midi*/
				i = 0;
				for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
					assert (_rmidi_in.size () > i);
					AlsaMidiIn* rm   = _rmidi_in.at (i);
					void*       bptr = (*it)->get_buffer (0);
					pframes_t   time;
					uint8_t     data[MaxAlsaMidiEventSize];
					size_t      size = sizeof (data);
					midi_clear (bptr);
					while (rm->recv_event (time, data, size)) {
						midi_event_put (bptr, time, data, size);
						size = sizeof (data);
					}
					rm->sync_time (clock1);
				}
				pthread_mutex_unlock (&_device_port_mutex);

				for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
					memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
				}

				/* call engine process callback */
				_last_process_start = g_get_monotonic_time ();
				if (engine.process_callback (_samples_per_period)) {
					_pcmi->pcm_stop ();
					_active = false;
					return 0;
				}

				/* only used when adding/removing MIDI device/system ports */
				pthread_mutex_lock (&_device_port_mutex);
				for (std::vector<BackendPortPtr>::iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
					boost::dynamic_pointer_cast<AlsaMidiPort> (*it)->next_period ();
				}

				/* queue outgoing midi */
				i = 0;
				for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it, ++i) {
					assert (_rmidi_out.size () > i);
					AlsaMidiBuffer const* src = boost::dynamic_pointer_cast<const AlsaMidiPort> (*it)->const_buffer ();
					AlsaMidiOut*          rm  = _rmidi_out.at (i);
					rm->sync_time (clock1);
					for (AlsaMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
						rm->send_event (mit->timestamp (), mit->data (), mit->size ());
					}
				}
				pthread_mutex_unlock (&_device_port_mutex);

				/* write back audio */
				i = 0;
				_pcmi->play_init (_samples_per_period);
				for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
					_pcmi->play_chan (i, (const float*)(*it)->get_buffer (_samples_per_period), _samples_per_period);
				}
				for (; i < _pcmi->nplay (); ++i) {
					_pcmi->clear_chan (i, _samples_per_period);
				}
				_pcmi->play_done (_samples_per_period);

				for (AudioSlaves::iterator s = _slaves.begin (); s != _slaves.end (); ++s) {
					if (!(*s)->active) {
						continue;
					}
					i = 0;
					for (std::vector<BackendPortPtr>::const_iterator it = (*s)->outputs.begin (); it != (*s)->outputs.end (); ++it, ++i) {
						(*s)->play_chan (i, (float*)(*it)->get_buffer (_samples_per_period), _samples_per_period);
					}
					(*s)->cycle_end ();
				}

				nr -= _samples_per_period;
				_processed_samples += _samples_per_period;

				_dsp_load_calc.set_max_time (_samplerate, _samples_per_period);
				_dsp_load_calc.set_start_timestamp_us (clock1);
				_dsp_load_calc.set_stop_timestamp_us (g_get_monotonic_time ());
				_dsp_load = _dsp_load_calc.get_dsp_load ();
				++last_n_periods;

				dsp_stats[RunLoop].update ();
			}

			if (xrun && (_pcmi->capt_xrun () > 0 || _pcmi->play_xrun () > 0)) {
				engine.Xrun ();
				reset_dll = true;
#if 0
				fprintf(stderr, "ALSA xrun read: %.2f ms, write: %.2f ms\n",
						_pcmi->capt_xrun() * 1000.0, _pcmi->play_xrun() * 1000.0);
#endif
			}
		} else {
			// Freewheelin'

			// zero audio input buffers
			for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
				memset ((*it)->get_buffer (_samples_per_period), 0, _samples_per_period * sizeof (Sample));
			}

			clock1     = g_get_monotonic_time ();
			uint32_t i = 0;
			pthread_mutex_lock (&_device_port_mutex);
			for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++i) {
				static_cast<AlsaMidiBuffer*> ((*it)->get_buffer (0))->clear ();
				AlsaMidiIn* rm   = _rmidi_in.at (i);
				void*       bptr = (*it)->get_buffer (0);
				midi_clear (bptr); // zero midi buffer

				// TODO add an API call for this.
				pframes_t time;
				uint8_t   data[64]; // match MaxAlsaEventSize in alsa_rawmidi.cc
				size_t    size = sizeof (data);
				while (rm->recv_event (time, data, size)) {
					; // discard midi-data from HW.
				}
				rm->sync_time (clock1);
			}
			pthread_mutex_unlock (&_device_port_mutex);

			_last_process_start = 0;
			if (engine.process_callback (_samples_per_period)) {
				_pcmi->pcm_stop ();
				_active = false;
				return 0;
			}

			// drop all outgoing MIDI messages
			pthread_mutex_lock (&_device_port_mutex);
			for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
				void* bptr = (*it)->get_buffer (0);
				midi_clear (bptr);
			}
			pthread_mutex_unlock (&_device_port_mutex);

			_dsp_load = 1.0;
			reset_dll = true;
			Glib::usleep (100); // don't hog cpu
		}

		bool connections_changed = false;
		bool ports_changed       = false;
		if (!pthread_mutex_trylock (&_port_callback_mutex)) {
			if (g_atomic_int_compare_and_exchange (&_port_change_flag, 1, 0)) {
				ports_changed = true;
			}
			if (!_port_connection_queue.empty ()) {
				connections_changed = true;
			}
			while (!_port_connection_queue.empty ()) {
				PortConnectData* c = _port_connection_queue.back ();
				manager.connect_callback (c->a, c->b, c->c);
				_port_connection_queue.pop_back ();
				delete c;
			}
			pthread_mutex_unlock (&_port_callback_mutex);
		}
		if (ports_changed) {
			manager.registration_callback ();
		}
		if (connections_changed) {
			manager.graph_order_callback ();
		}
		if (connections_changed || ports_changed) {
			update_system_port_latencies (); // flush, clear
			engine.latency_callback (false);
			engine.latency_callback (true);
		}
	}
	_pcmi->pcm_stop ();
	_active = false;
	if (_run) {
		engine.halted_callback ("ALSA I/O error.");
	}
	return 0;
}

/******************************************************************************/

bool
AlsaAudioBackend::add_slave (const char*            device,
                             unsigned int           slave_rate,
                             unsigned int           slave_spp,
                             unsigned int           slave_ppc,
                             AudioSlave::DuplexMode duplex)
{
	AudioSlave* s = new AudioSlave (device, duplex,
	                                _samplerate, _samples_per_period,
	                                slave_rate, slave_spp, slave_ppc);

	if (s->state ()) {
		// TODO parse error status
		PBD::error << string_compose (_("Failed to create slave device '%1' error %2\n"), device, s->state ()) << endmsg;
		goto errout;
	}

	for (uint32_t i = 0, n = 1; i < s->ncapt (); ++i) {
		char tmp[64];
		do {
			snprintf (tmp, sizeof (tmp), "extern:capture_%d", n);
			if (find_port (tmp)) {
				++n;
			} else {
				break;
			}
		} while (1);
		PortPtr p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsOutput | IsPhysical | IsTerminal));
		if (!p) {
			goto errout;
		}
		BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
		ap->set_hw_port_name (string_compose (_("Aux In %1"), n));
		s->inputs.push_back (ap);
	}

	for (uint32_t i = 0, n = 1; i < s->nplay (); ++i) {
		char tmp[64];
		do {
			snprintf (tmp, sizeof (tmp), "extern:playback_%d", n);
			if (find_port (tmp)) {
				++n;
			} else {
				break;
			}
		} while (1);
		PortPtr p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsInput | IsPhysical | IsTerminal));
		if (!p) {
			goto errout;
		}
		BackendPortPtr ap = boost::dynamic_pointer_cast<BackendPort> (p);
		ap->set_hw_port_name (string_compose (_("Aux Out %1"), n));
		s->outputs.push_back (ap);
	}

	if (!s->start ()) {
		PBD::error << string_compose (_("Failed to start slave device '%1'\n"), device) << endmsg;
		goto errout;
	}
	s->UpdateLatency.connect_same_thread (s->latency_connection, boost::bind (&AlsaAudioBackend::update_latencies, this));
	_slaves.push_back (s);
	return true;

errout:
	delete s; // releases device
	return false;
}

AlsaAudioBackend::AudioSlave::AudioSlave (
    const char*  device,
    DuplexMode   duplex,
    unsigned int master_rate,
    unsigned int master_samples_per_period,
    unsigned int slave_rate,
    unsigned int slave_samples_per_period,
    unsigned int slave_periods_per_cycle)
	: AlsaDeviceReservation (device)
	, AlsaAudioSlave (
			(duplex & HalfDuplexOut) ? device : NULL /* playback */,
			(duplex & HalfDuplexIn)  ? device : NULL /* capture */,
			master_rate, master_samples_per_period,
			slave_rate, slave_samples_per_period, slave_periods_per_cycle)
	, active (false)
	, halt (false)
	, dead (false)
{
	Halted.connect_same_thread (_halted_connection, boost::bind (&AudioSlave::halted, this));
}

AlsaAudioBackend::AudioSlave::~AudioSlave ()
{
	stop ();
}

void
AlsaAudioBackend::AudioSlave::halted ()
{
	// Note: Halted() is emitted from the Slave's process thread.
	release_device ();
	halt = true;
}

void
AlsaAudioBackend::AudioSlave::update_latencies (uint32_t play, uint32_t capt)
{
	LatencyRange lr;
	lr.min = lr.max = (capt);
	for (std::vector<BackendPortPtr>::const_iterator it = inputs.begin (); it != inputs.end (); ++it) {
		(*it)->set_latency_range (lr, false);
	}

	lr.min = lr.max = play;
	for (std::vector<BackendPortPtr>::const_iterator it = outputs.begin (); it != outputs.end (); ++it) {
		(*it)->set_latency_range (lr, true);
	}
#ifndef NDEBUG
	printf ("ALSA SLAVE-device latency play=%d capt=%d\n", play, capt); // XXX DEBUG
#endif
	UpdateLatency (); /* EMIT SIGNAL */
}

/******************************************************************************/

static boost::shared_ptr<AlsaAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int  instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int  deinstantiate ();
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
/******************************************************************************/

AlsaAudioPort::AlsaAudioPort (AlsaAudioBackend& b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock (_buffer, sizeof (_buffer));
}

AlsaAudioPort::~AlsaAudioPort ()
{
}

void*
AlsaAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		const std::set<BackendPortPtr>& connections = get_connections ();
		std::set<BackendPortPtr>::const_iterator it = connections.begin ();
		if (it == connections.end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			boost::shared_ptr<const AlsaAudioPort> source = boost::dynamic_pointer_cast<const AlsaAudioPort> (*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != connections.end ()) {
				source = boost::dynamic_pointer_cast<const AlsaAudioPort> (*it);
				assert (source && source->is_output ());
				Sample*       dst = buffer ();
				const Sample* src = source->const_buffer ();
				for (uint32_t s = 0; s < n_samples; ++s, ++dst, ++src) {
					*dst += *src;
				}
			}
		}
	}
	return _buffer;
}

AlsaMidiPort::AlsaMidiPort (AlsaAudioBackend& b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
	, _n_periods (1)
	, _bufperiod (0)
{
	_buffer[0].clear ();
	_buffer[1].clear ();
	_buffer[2].clear ();

	_buffer[0].reserve (256);
	_buffer[1].reserve (256);
	_buffer[2].reserve (256);
}

AlsaMidiPort::~AlsaMidiPort ()
{
}

struct MidiEventSorter {
	bool operator() (AlsaMidiEvent const& a, AlsaMidiEvent const& b)
	{
		return a < b;
	}
};

void* AlsaMidiPort::get_buffer (pframes_t /* nframes */)
{
	if (is_input ()) {
		(_buffer[_bufperiod]).clear ();
		const std::set<BackendPortPtr>& connections = get_connections ();
		for (std::set<BackendPortPtr>::const_iterator i = connections.begin ();
		     i != connections.end ();
		     ++i) {
			const AlsaMidiBuffer* src = boost::dynamic_pointer_cast<const AlsaMidiPort> (*i)->const_buffer ();
			for (AlsaMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				(_buffer[_bufperiod]).push_back (*it);
			}
		}
		std::stable_sort ((_buffer[_bufperiod]).begin (), (_buffer[_bufperiod]).end (), MidiEventSorter ());
	}
	return &(_buffer[_bufperiod]);
}

AlsaMidiEvent::AlsaMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
{
	if (size > 0 && size < MaxAlsaMidiEventSize) {
		memcpy (_data, data, size);
	}
}

AlsaMidiEvent::AlsaMidiEvent (const AlsaMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
{
	if (other._size > 0) {
		assert (other._size < MaxAlsaMidiEventSize);
		memcpy (_data, other._data, other._size);
	}
};

/******************************************************************************/

AlsaDeviceReservation::AlsaDeviceReservation ()
	: _device_reservation (0)
{
}

AlsaDeviceReservation::AlsaDeviceReservation (const char* device_name)
	: _device_reservation (0)
{
	acquire_device (device_name);
}

AlsaDeviceReservation::~AlsaDeviceReservation ()
{
	release_device ();
}

bool
AlsaDeviceReservation::acquire_device (const char* device_name)
{
	int device_number = card_to_num (device_name);
	if (device_number < 0) {
		return false;
	}

	assert (_device_reservation == 0);
	_reservation_succeeded = false;

	std::string request_device_exe;
	if (!PBD::find_file (
	        PBD::Searchpath (Glib::build_filename (ARDOUR::ardour_dll_directory (), "ardouralsautil") + G_SEARCHPATH_SEPARATOR_S + ARDOUR::ardour_dll_directory ()),
	        "ardour-request-device", request_device_exe)) {
		PBD::warning << "ardour-request-device binary was not found..'" << endmsg;
		return false;
	}

	char** argp;
	char   tmp[128];
	argp    = (char**)calloc (5, sizeof (char*));
	argp[0] = strdup (request_device_exe.c_str ());
	argp[1] = strdup ("-P");
	snprintf (tmp, sizeof (tmp), "%d", getpid ());
	argp[2] = strdup (tmp);
	snprintf (tmp, sizeof (tmp), "Audio%d", device_number);
	argp[3] = strdup (tmp);
	argp[4] = 0;

	_device_reservation = new ARDOUR::SystemExec (request_device_exe, argp);
	_device_reservation->ReadStdout.connect_same_thread (_reservation_connection, boost::bind (&AlsaDeviceReservation::reservation_stdout, this, _1, _2));
	_device_reservation->Terminated.connect_same_thread (_reservation_connection, boost::bind (&AlsaDeviceReservation::release_device, this));

	if (_device_reservation->start (SystemExec::ShareWithParent)) {
		PBD::warning << _("AlsaAudioBackend: Device Request failed.") << endmsg;
		release_device ();
		return false;
	}

	/* wait to check if reservation suceeded. */
	int timeout = 500; // 5 sec
	while (_device_reservation && !_reservation_succeeded && --timeout > 0) {
		Glib::usleep (10000);
	}

	if (timeout == 0 || !_reservation_succeeded) {
		PBD::warning << _("AlsaAudioBackend: Device Reservation failed.") << endmsg;
		release_device ();
		return false;
	}
	return true;
}

void
AlsaDeviceReservation::release_device ()
{
	_reservation_connection.drop_connections ();
	ARDOUR::SystemExec* tmp = _device_reservation;
	_device_reservation     = 0;
	delete tmp;
}

void
AlsaDeviceReservation::reservation_stdout (std::string d, size_t /* s */)
{
	if (d.substr (0, 19) == "Acquired audio-card") {
		_reservation_succeeded = true;
	}
}
