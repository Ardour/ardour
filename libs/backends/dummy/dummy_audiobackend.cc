/*
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 John Emmas <john@creativepost.co.uk>
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

#include <math.h>
#include <sys/time.h>
#include <regex.h>
#include <stdlib.h>

#include <glibmm.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <pbd/windows_timer_utils.h>
#endif

#include "dummy_audiobackend.h"
#include "dummy_midi_seq.h"

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/pthread_utils.h"

#include "ardour/port_manager.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t DummyAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> DummyAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> DummyAudioBackend::_device_status;

std::vector<DummyAudioBackend::DriverSpeed> DummyAudioBackend::_driver_speed;

static int64_t _x_get_monotonic_usec() {
#ifdef PLATFORM_WINDOWS
	return PBD::get_microseconds();
#endif
	return g_get_monotonic_time();
}

DummyAudioBackend::DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, PortEngineSharedImpl (e, s_instance_name)
	, _running (false)
	, _freewheel (false)
	, _freewheeling (false)
	, _speedup (1.0)
	, _device ("")
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _dsp_load (0)
	, _n_inputs (0)
	, _n_outputs (0)
	, _n_midi_inputs (0)
	, _n_midi_outputs (0)
	, _midi_mode (MidiNoEvents)
	, _systemic_input_latency (0)
	, _systemic_output_latency (0)
	, _processed_samples (0)
{
	_instance_name = s_instance_name;
	_device = _("Silence");

	if (_driver_speed.empty()) {
		_driver_speed.push_back (DriverSpeed (_("Half Speed"),   2.0f));
		_driver_speed.push_back (DriverSpeed (_("Normal Speed"), 1.0f));
		_driver_speed.push_back (DriverSpeed (_("Double Speed"), 0.5f));
		_driver_speed.push_back (DriverSpeed (_("5x Speed"),     0.2f));
		_driver_speed.push_back (DriverSpeed (_("10x Speed"),    0.1f));
		_driver_speed.push_back (DriverSpeed (_("15x Speed"),    0.06666f));
		_driver_speed.push_back (DriverSpeed (_("20x Speed"),    0.05f));
		_driver_speed.push_back (DriverSpeed (_("50x Speed"),    0.02f));
	}

}

DummyAudioBackend::~DummyAudioBackend ()
{
	clear_ports ();
}

/* AUDIOBACKEND API */

std::string
DummyAudioBackend::name () const
{
	return X_("Dummy"); // internal name
}

bool
DummyAudioBackend::is_realtime () const
{
	return false;
}

std::vector<AudioBackend::DeviceStatus>
DummyAudioBackend::enumerate_devices () const
{
	if (_device_status.empty()) {
		_device_status.push_back (DeviceStatus (_("Silence"), true));
		_device_status.push_back (DeviceStatus (_("DC -6dBFS (+.5)"), true));
		_device_status.push_back (DeviceStatus (_("Demolition"), true));
		_device_status.push_back (DeviceStatus (_("Sine Wave"), true));
		_device_status.push_back (DeviceStatus (_("Sine Wave 1K, 1/3 Oct"), true));
		_device_status.push_back (DeviceStatus (_("Square Wave"), true));
		_device_status.push_back (DeviceStatus (_("Impulses"), true));
		_device_status.push_back (DeviceStatus (_("Uniform White Noise"), true));
		_device_status.push_back (DeviceStatus (_("Gaussian White Noise"), true));
		_device_status.push_back (DeviceStatus (_("Pink Noise"), true));
		_device_status.push_back (DeviceStatus (_("Pink Noise (low CPU)"), true));
		_device_status.push_back (DeviceStatus (_("Sine Sweep"), true));
		_device_status.push_back (DeviceStatus (_("Sine Sweep Swell"), true));
		_device_status.push_back (DeviceStatus (_("Square Sweep"), true));
		_device_status.push_back (DeviceStatus (_("Square Sweep Swell"), true));
		_device_status.push_back (DeviceStatus (_("Engine Pulse"), true));
		_device_status.push_back (DeviceStatus (_("LTC"), true));
		_device_status.push_back (DeviceStatus (_("Loopback"), true));
	}
	return _device_status;
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
	return false;
}

bool
DummyAudioBackend::can_change_buffer_size_when_running () const
{
	return true;
}

std::vector<std::string>
DummyAudioBackend::enumerate_drivers () const
{
	std::vector<std::string> speed_drivers;
	for (std::vector<DriverSpeed>::const_iterator it = _driver_speed.begin () ; it != _driver_speed.end (); ++it) {
		speed_drivers.push_back (it->name);
	}
	return speed_drivers;
}

std::string
DummyAudioBackend::driver_name () const
{
	for (std::vector<DriverSpeed>::const_iterator it = _driver_speed.begin () ; it != _driver_speed.end (); ++it) {
		if (rintf (1e6f * _speedup) == rintf (1e6f * it->speedup)) {
			return it->name;
		}
	}
	assert (0);
	return _("Normal Speed");
}

int
DummyAudioBackend::set_driver (const std::string& d)
{
	for (std::vector<DriverSpeed>::const_iterator it = _driver_speed.begin () ; it != _driver_speed.end (); ++it) {
		if (d == it->name) {
			_speedup = it->speedup;
			return 0;
		}
	}
	assert (0);
	return -1;
}

int
DummyAudioBackend::set_device_name (const std::string& d)
{
	_device = d;
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
	if (bs <= 0 || bs > _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;

	/* update port latencies
	 * with 'Loopback' there is exactly once cycle latency,
	 * divide it between In + Out;
	 */
	LatencyRange lr;
	lr.min = lr.max = _systemic_input_latency;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
		set_latency_range (*it, false, lr);
	}
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
		set_latency_range (*it, false, lr);
	}

	lr.min = lr.max = _systemic_output_latency;
	for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		set_latency_range (*it, true, lr);
	}
	for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
		set_latency_range (*it, true, lr);
	}

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
	return _device;
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
	if (_midi_options.empty()) {
		_midi_options.push_back (_("1 in, 1 out, Silence"));
		_midi_options.push_back (_("2 in, 2 out, Silence"));
		_midi_options.push_back (_("8 in, 8 out, Silence"));
		_midi_options.push_back (_("Midi Event Generators"));
		_midi_options.push_back (_("Engine Pulse"));
		_midi_options.push_back (_("8 in, 8 out, Loopback"));
		_midi_options.push_back (_("MIDI to Audio, Loopback"));
		_midi_options.push_back (_("No MIDI I/O"));
	}
	return _midi_options;
}

int
DummyAudioBackend::set_midi_option (const std::string& opt)
{
	_midi_mode = MidiNoEvents;
	if (opt == _("1 in, 1 out, Silence")) {
		_n_midi_inputs = _n_midi_outputs = 1;
	}
	else if (opt == _("2 in, 2 out, Silence")) {
		_n_midi_inputs = _n_midi_outputs = 2;
	}
	else if (opt == _("8 in, 8 out, Silence")) {
		_n_midi_inputs = _n_midi_outputs = 8;
	}
	else if (opt == _("Engine Pulse")) {
		_n_midi_inputs = _n_midi_outputs = 1;
		_midi_mode = MidiOneHz;
	}
	else if (opt == _("Midi Event Generators")) {
		_n_midi_inputs = _n_midi_outputs = NUM_MIDI_EVENT_GENERATORS;
		_midi_mode = MidiGenerator;
	}
	else if (opt == _("8 in, 8 out, Loopback")) {
		_n_midi_inputs = _n_midi_outputs = 8;
		_midi_mode = MidiLoopback;
	}
	else if (opt == _("MIDI to Audio, Loopback")) {
		_n_midi_inputs = _n_midi_outputs = UINT32_MAX;
		_midi_mode = MidiToAudio;
	}
	else {
		_n_midi_inputs = _n_midi_outputs = 0;
	}
	return 0;
}

std::string
DummyAudioBackend::midi_option () const
{
	return ""; // TODO
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
		return BackendReinitializationError;
	}

	clear_ports ();

	if (register_system_ports()) {
		PBD::error << _("DummyAudioBackend: failed to register system ports.") << endmsg;
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("DummyAudioBackend: Could not re-establish ports.") << endmsg;
		stop ();
		return PortReconnectError;
	}

	engine.reconnect_ports ();
	g_atomic_int_set (&_port_change_flag, 0);

	if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &_main_thread, pthread_process, this)) {
		PBD::error << _("DummyAudioBackend: cannot start.") << endmsg;
	}

	int timeout = 5000;
	while (!_running && --timeout > 0) { Glib::usleep (1000); }

	if (timeout == 0 || !_running) {
		PBD::error << _("DummyAudioBackend: failed to start process thread.") << endmsg;
		return ProcessThreadStartError;
	}

	return NoError;
}

int
DummyAudioBackend::stop ()
{
	void *status;
	if (!_running) {
		return 0;
	}

	_running = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("DummyAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}
	unregister_ports();
	return 0;
}

int
DummyAudioBackend::freewheel (bool onoff)
{
	_freewheeling = onoff;
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
			return _samples_per_period * sizeof(Sample);
		case DataType::MIDI:
			return _max_buffer_size; // XXX not really limited
	}
	return 0;
}

/* Process time */
samplepos_t
DummyAudioBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
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
	pthread_t   thread_id;
	ThreadData* td = new ThreadData (this, func, PBD_RT_STACKSIZE_PROC);

	if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &thread_id, dummy_process_thread, td)) {
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
DummyAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
DummyAudioBackend::update_latencies ()
{
	// trigger latency callback in RT thread (locked graph)
	port_connect_add_remove_callback();
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

int
DummyAudioBackend::register_system_ports()
{
	LatencyRange lr;
	enum DummyAudioPort::GeneratorType gt;
	if (_device == _("Uniform White Noise")) {
		gt = DummyAudioPort::UniformWhiteNoise;
	} else if (_device == _("Gaussian White Noise")) {
		gt = DummyAudioPort::GaussianWhiteNoise;
	} else if (_device == _("Pink Noise")) {
		gt = DummyAudioPort::PinkNoise;
	} else if (_device == _("Pink Noise (low CPU)")) {
		gt = DummyAudioPort::PonyNoise;
	} else if (_device == _("Sine Wave")) {
		gt = DummyAudioPort::SineWave;
	} else if (_device == _("Sine Wave 1K, 1/3 Oct")) {
		gt = DummyAudioPort::SineWaveOctaves;
	} else if (_device == _("Square Wave")) {
		gt = DummyAudioPort::SquareWave;
	} else if (_device == _("Impulses")) {
		gt = DummyAudioPort::KronekerDelta;
	} else if (_device == _("Sine Sweep")) {
		gt = DummyAudioPort::SineSweep;
	} else if (_device == _("Sine Sweep Swell")) {
		gt = DummyAudioPort::SineSweepSwell;
	} else if (_device == _("Square Sweep")) {
		gt = DummyAudioPort::SquareSweep;
	} else if (_device == _("Square Sweep Swell")) {
		gt = DummyAudioPort::SquareSweepSwell;
	} else if (_device == _("Engine Pulse")) {
		gt = DummyAudioPort::OneHz;
	} else if (_device == _("LTC")) {
		gt = DummyAudioPort::LTC;
	} else if (_device == _("Loopback")) {
		gt = DummyAudioPort::Loopback;
	} else if (_device == _("Demolition")) {
		gt = DummyAudioPort::Demolition;
	} else if (_device == _("DC -6dBFS (+.5)")) {
		gt = DummyAudioPort::DC05;
	} else {
		gt = DummyAudioPort::Silence;
	}

	if (_midi_mode == MidiToAudio) {
		gt = DummyAudioPort::Loopback;
	}

	const int a_ins = _n_inputs > 0 ? _n_inputs : 8;
	const int a_out = _n_outputs > 0 ? _n_outputs : 8;
	const int m_ins = _n_midi_inputs == UINT_MAX ? 0 : _n_midi_inputs;
	const int m_out = _n_midi_outputs == UINT_MAX ? a_ins : _n_midi_outputs;


	/* audio ports */
	lr.min = lr.max = _systemic_input_latency;
	for (int i = 1; i <= a_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:capture_%d", i);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);

		boost::shared_ptr<DummyAudioPort> dp = boost::dynamic_pointer_cast<DummyAudioPort>(p);

		_system_inputs.push_back (dp);

		std::string name = dp->setup_generator (gt, _samplerate, i - 1, a_ins);

		if (!name.empty ()) {
			dp->set_hw_port_name (name);
		}
	}

	lr.min = lr.max = _systemic_output_latency;
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i);
		PortPtr p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_outputs.push_back (boost::dynamic_pointer_cast<BackendPort>(p));
	}

	/* midi ports */
	lr.min = lr.max = _systemic_input_latency;
	for (int i = 0; i < m_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_capture_dummy_%d", i+1);
		PortPtr p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);

		boost::shared_ptr<DummyMidiPort> dp = boost::dynamic_pointer_cast<DummyMidiPort>(p);

		_system_midi_in.push_back (dp);

		if (_midi_mode == MidiGenerator) {
			std::string name = dp->setup_generator (i % NUM_MIDI_EVENT_GENERATORS, _samplerate);
			if (!name.empty ()) {
				dp->set_hw_port_name (name);
			}
		}
		else if (_midi_mode == MidiOneHz) {
			std::string name = dp->setup_generator (-1, _samplerate);
			if (!name.empty ()) {
				dp->set_hw_port_name (name);
			}
		}
	}

	lr.min = lr.max = _systemic_output_latency;
	for (int i = 1; i <= m_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_playback_dummy_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);

		boost::shared_ptr<DummyMidiPort> dp = boost::dynamic_pointer_cast<DummyMidiPort>(p);
		_system_midi_out.push_back (dp);

		if (_device == _("Loopback") && _midi_mode == MidiToAudio) {
			std::stringstream ss;
			ss << "Midi2Audio";
			for (int apc = 0; apc < (int)_system_inputs.size(); ++apc) {
				if ((apc % m_out) + 1 == i) {
					ss << " >" << (apc + 1);
				}
			}
			dp->set_hw_port_name (ss.str());
		}
	}
	return 0;
}

BackendPort*
DummyAudioBackend::port_factory (std::string const & name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	BackendPort* port = 0;

	switch (type) {
		case DataType::AUDIO:
			port = new DummyAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new DummyMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << string_compose (_("%1::register_port: Invalid Data Type."), _instance_name) << endmsg;
			return 0;
	}

	return port;
}

/* MIDI */
int
DummyAudioBackend::midi_event_get (
		pframes_t& timestamp,
		size_t& size, uint8_t const** buf, void* port_buffer,
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
		// nevermind, ::get_buffer() sorts events, but always print warning
		fprintf (stderr, "DummyMidiBuffer: it's too late for this event %d > %d.\n", (pframes_t)dst.back ()->timestamp (), timestamp);
	}
	dst.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (timestamp, buffer, size)));
#if 0 // DEBUG MIDI EVENTS
	printf("DummyAudioBackend::midi_event_put %d, %zu: ", timestamp, size);
	for (size_t xx = 0; xx < size; ++xx) {
		printf(" %02x", buffer[xx]);
	}
	printf("\n");
#endif
	return 0;
}

uint32_t
DummyAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<DummyMidiBuffer*>(port_buffer)->size ();
}

void
DummyAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
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
DummyAudioBackend::set_latency_range (PortEngine::PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::set_latency_range (): invalid port.") << endmsg;
	}
	port->set_latency_range (latency_range, for_playback);
}

LatencyRange
DummyAudioBackend::get_latency_range (PortEngine::PortHandle port_handle, bool for_playback)
{
	LatencyRange r;
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}

	r = port->latency_range (for_playback);
#ifndef ZERO_LATENCY
	if (port->is_physical() && port->is_terminal()) {
		if (port->is_input() && for_playback) {
			const size_t l_in = _samples_per_period * .25;
			r.min += l_in;
			r.max += l_in;
		}
		if (port->is_output() && !for_playback) {
			/* with 'Loopback' there is exactly once cycle latency, divide it between In + Out; */
			const size_t l_in = _samples_per_period * .25;
			const size_t l_out = _samples_per_period - l_in;
			r.min += l_out;
			r.max += l_out;
		}
	}
#endif
	return r;
}

/* Getting access to the data buffer for a port */

void*
DummyAudioBackend::get_buffer (PortEngine::PortHandle port_handle, pframes_t nframes)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	assert (port);
	assert (valid_port (port));
	return port->get_buffer (nframes);
}

/* Engine Process */
void *
DummyAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_running = true;
	_processed_samples = 0;

	manager.registration_callback();
	manager.graph_order_callback();

	int64_t clock1;
	clock1 = -1;
	while (_running) {
		const size_t samples_per_period = _samples_per_period;

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
		}

		// re-set input buffers, generate on demand.
		for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			boost::dynamic_pointer_cast<DummyPort>(*it)->next_period ();
		}
		for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
			boost::dynamic_pointer_cast<DummyPort>(*it)->next_period ();
		}

		if (engine.process_callback (samples_per_period)) {
			return 0;
		}
		_processed_samples += samples_per_period;

		if (_device == _("Loopback") && _midi_mode != MidiToAudio) {
			int opn = 0;
			int opc = _system_outputs.size();
			for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++opn) {
				BackendPortPtr op = _system_outputs[(opn % opc)];
				boost::dynamic_pointer_cast<DummyAudioPort>(*it)->fill_wavetable ((const float*)op->get_buffer (samples_per_period), samples_per_period);
			}
		}

		if (_midi_mode == MidiLoopback) {
			int opn = 0;
			int opc = _system_midi_out.size();
			for (std::vector<BackendPortPtr>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++opn) {
				boost::shared_ptr<DummyMidiPort> op = boost::dynamic_pointer_cast<DummyMidiPort> (_system_midi_out[(opn % opc)]);
				op->get_buffer(0); // mix-down
				boost::dynamic_pointer_cast<DummyMidiPort>(*it)->set_loopback (op->const_buffer());
			}
		}
		else if (_midi_mode == MidiToAudio) {
			int opn = 0;
			int opc = _system_midi_out.size();
			for (std::vector<BackendPortPtr>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++opn) {
				boost::shared_ptr<DummyMidiPort> op = boost::dynamic_pointer_cast<DummyMidiPort> (_system_midi_out[(opn % opc)]);
				op->get_buffer(0); // mix-down
				boost::dynamic_pointer_cast<DummyAudioPort>(*it)->midi_to_wavetable (op->const_buffer(), samples_per_period);
			}
		}

		if (!_freewheel) {
			_dsp_load_calc.set_max_time (_samplerate, samples_per_period);
			_dsp_load_calc.set_start_timestamp_us (clock1);
			_dsp_load_calc.set_stop_timestamp_us (_x_get_monotonic_usec());
			_dsp_load = _dsp_load_calc.get_dsp_load_unbound ();

			const int64_t elapsed_time = _dsp_load_calc.elapsed_time_us ();
			const int64_t nominal_time = _dsp_load_calc.get_max_time_us ();
			if (elapsed_time < nominal_time) {
				const int64_t sleepy = _speedup * (nominal_time - elapsed_time);
				Glib::usleep (std::max ((int64_t) 100, sleepy));
			} else {
				Glib::usleep (100); // don't hog cpu
			}
		} else {
			_dsp_load = 1.0f;
			Glib::usleep (100); // don't hog cpu
		}

		/* beginning of next cycle */
		clock1 = _x_get_monotonic_usec();

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
	_running = false;
	return 0;
}


/******************************************************************************/

static boost::shared_ptr<DummyAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	_("None (Dummy)"),
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
	// special-case: unit-tests require backend to be pre-configured.
	if (s_instance_name == "Unit-Test") {
		return true;
	}
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
DummyPort::DummyPort (DummyAudioBackend &b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
	, _rseed (0)
	, _gen_cycle (false)
	, _engine (b)
{
}

DummyPort::~DummyPort ()
{
}

void DummyPort::setup_random_number_generator ()
{
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER Count;
	if (QueryPerformanceCounter (&Count)) {
		_rseed = Count.QuadPart;
	} else
#endif
	{
	_rseed = g_get_monotonic_time();
	}
	_rseed = (_rseed + (uint64_t)this) % INT_MAX;
	if (_rseed == 0) _rseed = 1;
}

inline uint32_t
DummyPort::randi ()
{
	// 31bit Park-Miller-Carta Pseudo-Random Number Generator
	// http://www.firstpr.com.au/dsp/rand31/
	uint32_t hi, lo;
	lo = 16807 * (_rseed & 0xffff);
	hi = 16807 * (_rseed >> 16);

	lo += (hi & 0x7fff) << 16;
	lo += hi >> 15;
#if 1
	lo = (lo & 0x7fffffff) + (lo >> 31);
#else
	if (lo > 0x7fffffff) { lo -= 0x7fffffff; }
#endif
	return (_rseed = lo);
}

inline float
DummyPort::randf ()
{
	return (randi() / 1073741824.f) - 1.f;
}

pframes_t
DummyPort::pulse_position () const
{
	samplecnt_t sr = _engine.sample_rate ();
	samplepos_t st = _engine.sample_time_at_cycle_start();
	return (sr - (st % sr)) % sr;
}

/******************************************************************************/

DummyAudioPort::DummyAudioPort (DummyAudioBackend &b, const std::string& name, PortFlags flags)
	: DummyPort (b, name, flags)
	, _gen_type (Silence)
	, _b0 (0)
	, _b1 (0)
	, _b2 (0)
	, _b3 (0)
	, _b4 (0)
	, _b5 (0)
	, _b6 (0)
	, _wavetable (0)
	, _gen_period (0)
	, _gen_offset (0)
	, _gen_perio2 (0)
	, _gen_count2 (0)
	, _pass (false)
	, _rn1 (0)
	, _ltc (0)
	, _ltcbuf (0)
{
	memset (_buffer, 0, sizeof (_buffer));
}

DummyAudioPort::~DummyAudioPort () {
	free(_wavetable);
	ltc_encoder_free (_ltc);
	delete _ltcbuf;
	_wavetable = 0;
	_ltc = 0;
	_ltcbuf = 0;
}

static std::string format_hz (float freq) {
	std::stringstream ss;
	if (freq >= 10000) {
		ss <<  std::setprecision (1) << std::fixed << freq / 1000 << "kHz";
	} else if (freq >= 1000) {
		ss <<  std::setprecision (2) << std::fixed << freq / 1000 << "kHz";
	} else {
		ss <<  std::setprecision (1) << std::fixed << freq << "Hz";
	}
	return ss.str ();
}

static size_t fit_wave (float freq, float rate, float precision = 0.001) {
	const size_t max_mult = floor (freq * rate);
	float minErr = 2;
	size_t fact = 1;
	for (size_t i = 1; i < max_mult; ++i) {
		const float isc = rate * (float)i / freq; // ideal sample count
		const float rsc = rintf (isc); // rounded sample count
		const float err = fabsf (isc - rsc);
		if (err < minErr) {
			minErr = err;
			fact = i;
		}
		if (err < precision) {
			break;
		}
	}
	//printf(" FIT %8.1f Hz / %8.1f Hz * %ld = %.0f (err: %e)\n", freq, rate, fact, fact * rate / freq, minErr);
	return fact;
}

std::string
DummyAudioPort::setup_generator (GeneratorType const g, float const samplerate, int c, int total)
{
	std::string name;
	DummyPort::setup_random_number_generator();
	_gen_type = g;

	switch (_gen_type) {
		case PinkNoise:
		case PonyNoise:
		case UniformWhiteNoise:
		case GaussianWhiteNoise:
		case DC05:
		case Silence:
			break;
		case OneHz:
			name = string_compose ("One Hz (%1)", 1 + c);
			break;
		case Demolition:
			_gen_period = 3 * samplerate;
			break;
		case KronekerDelta:
			_gen_period = (5 + randi() % (int)(samplerate / 20.f));
			name = "Delta " + format_hz (samplerate / _gen_period);
			break;
		case SquareWave:
			_gen_period = (5 + randi() % (int)(samplerate / 20.f)) & ~1;
			name = "Square " + format_hz (samplerate / _gen_period);
			break;
		case SineWaveOctaves:
			{
				const int x = c - floor (((float)total / 2));
				float f = powf (2.f, x / 3.f) * 1000.f;
				f = std::max (10.f, std::min (samplerate *.5f, f));
				const size_t mult = fit_wave (f, samplerate);
				_gen_period = rintf ((float)mult * samplerate / f);
				name = "Sine " + format_hz (samplerate * mult / (float)_gen_period);
				_wavetable = (Sample*) malloc (_gen_period * sizeof(Sample));
				for (uint32_t i = 0 ; i < _gen_period; ++i) {
					_wavetable[i] = .12589f * sinf(2.0f * M_PI * (float)mult * (float)i / (float)(_gen_period)); // -18dBFS
				}
			}
			break;
		case SineWave:
			_gen_period = 5 + randi() % (int)(samplerate / 20.f);
			name = "Sine " + format_hz (samplerate / _gen_period);
			_wavetable = (Sample*) malloc (_gen_period * sizeof(Sample));
			for (uint32_t i = 0 ; i < _gen_period; ++i) {
				_wavetable[i] = .12589f * sinf(2.0f * M_PI * (float)i / (float)_gen_period); // -18dBFS
			}
			break;
		case SquareSweep:
		case SquareSweepSwell:
		case SineSweep:
		case SineSweepSwell:
			{
				_gen_period = 5 * samplerate + randi() % (int)(samplerate * 10.f);
				_gen_period &= ~1;
				_gen_perio2 = 1 | (int)ceilf (_gen_period * .89f); // Volume Swell period
				const double f_min = 20.;
				const double f_max = samplerate * .5;
				const double g_p2 = _gen_period * .5;
#ifdef LINEAR_SWEEP
				const double b = (f_max - f_min) / (2. * samplerate * g_p2);
				const double a = f_min / samplerate;
#else
				const double b = log (f_max / f_min) / g_p2;
				const double a = f_min / (b * samplerate);
#endif
				const uint32_t g_p2i = rint(g_p2);
				_wavetable = (Sample*) malloc (_gen_period * sizeof(Sample));
				for (uint32_t i = 0 ; i < g_p2i; ++i) {
#ifdef LINEAR_SWEEP
					const double phase = i * (a + b * i);
#else
					const double phase = a * exp (b * i) - a;
#endif
					_wavetable[i] = (float)sin (2. * M_PI * (phase - floor (phase)));
				}
				for (uint32_t i = g_p2i; i < _gen_period; ++i) {
					const uint32_t j = _gen_period - i;
#ifdef LINEAR_SWEEP
					const double phase = j * (a + b * j);
#else
					const double phase = a * exp (b * j) - a;
#endif
					_wavetable[i] = -(float)sin (2. * M_PI * (phase - floor (phase)));
				}
				if (_gen_type == SquareSweep) {
					for (uint32_t i = 0 ; i < _gen_period; ++i) {
						_wavetable[i] = _wavetable[i] < 0 ? -.40709f : .40709f;
					}
				}
				else if (_gen_type == SquareSweepSwell) {
					for (uint32_t i = 0 ; i < _gen_period; ++i) {
						_wavetable[i] = _wavetable[i] < 0 ? -1 : 1;
					}
				}
			}
			break;
		case LTC:
			switch (c % 4) {
				case 0:
					_ltc = ltc_encoder_create (samplerate, 25, LTC_TV_625_50, 0);
					name = "LTC25";
					break;
				case 1:
					_ltc = ltc_encoder_create (samplerate, 30, LTC_TV_1125_60, 0);
					name = "LTC30";
					break;
				case 2:
					_ltc = ltc_encoder_create (samplerate, 30001.f / 1001.f, LTC_TV_525_60, 0);
					name = "LTC29df";
					break;
				case 3:
					_ltc = ltc_encoder_create (samplerate, 24, LTC_TV_FILM_24, 0);
					name = "LTC24";
					break;
			}
			_ltc_spd = 1.0;
			_ltc_rand = floor((float)c / 4) * .001f;
			if (c < 4) {
					name += " (locked)";
			} else {
					name += " (varspd)";
			}
			SMPTETimecode tc;
			tc.years = 0;
			tc.months = 0;
			tc.days = 0;
			tc.hours = (3 * (c / 4)) % 24; // XXX
			tc.mins = 0;
			tc.secs = 0;
			tc.frame = 0;
			ltc_encoder_set_timecode (_ltc, &tc);
					name += string_compose ("@%1h", (int)tc.hours);
			_ltcbuf = new PBD::RingBuffer<Sample> (std::max (DummyAudioBackend::max_buffer_size() * 2.f, samplerate));
			break;
		case Loopback:
			_wavetable = (Sample*) calloc (DummyAudioBackend::max_buffer_size(), sizeof(Sample));
			break;
	}
	return name;
}

void DummyAudioPort::midi_to_wavetable (DummyMidiBuffer const * const src, size_t n_samples)
{
	memset(_wavetable, 0, n_samples * sizeof(float));
	/* generate an audio spike for every midi message
	 * to verify layency-compensation alignment
	 * (here: midi-out playback-latency + audio-in capture-latency)
	 */
	for (DummyMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
		const pframes_t t = (*it)->timestamp();
		assert(t < n_samples);
		// somewhat arbitrary mapping for quick visual feedback
		float v = -.5f;
		if ((*it)->size() == 3) {
			const unsigned char *d = (*it)->data();
			if ((d[0] & 0xf0) == 0x90) { // note on
				v = .25f + d[2] / 512.f;
			}
			else if ((d[0] & 0xf0) == 0x80) { // note off
				v = .3f - d[2] / 640.f;
			}
			else if ((d[0] & 0xf0) == 0xb0) { // CC
				v = -.1f - d[2] / 256.f;
			}
		}
		_wavetable[t] += v;
	}
}

float DummyAudioPort::grandf ()
{
	// Gaussian White Noise
	// http://www.musicdsp.org/archive.php?classid=0#109
	float x1, x2, r;

	if (_pass) {
		_pass = false;
		return _rn1;
	}

	do {
		x1 = randf ();
		x2 = randf ();
		r = x1 * x1 + x2 * x2;
	} while ((r >= 1.0f) || (r < 1e-22f));

	r = sqrtf (-2.f * logf (r) / r);

	_pass = true;
	_rn1 = r * x2;
	return r * x1;
}

/* inspired by jack-demolition by Steve Harris */
static const float _demolition[] = {
	 0.0f,             /* special case - 0dbFS white noise */
	 0.0f,             /* zero, may cause denomrals following a signal */
	 0.73 / 1e45,      /* very small - should be denormal when floated */
	 3.7f,             /* arbitrary number > 0dBFS */
	-4.3f,             /* arbitrary negative number > 0dBFS */
	 4294967395.0f,    /* 2^16 + 100 */
	-4294967395.0f,
	 3.402823466e+38F, /* HUGE, HUGEVALF, non-inf number */
	 INFINITY,         /* +inf */
	-INFINITY,         /* -inf */
	-NAN,              /* -nan */
	 NAN,              /*  nan */
	 0.0f,             /* some silence to check for recovery */
};

void DummyAudioPort::generate (const pframes_t n_samples)
{
	Glib::Threads::Mutex::Lock lm (generator_lock);
	if (_gen_cycle) {
		return;
	}

	switch (_gen_type) {
		case Silence:
			memset (_buffer, 0, n_samples * sizeof (Sample));
			break;
		case DC05:
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				_buffer[i] = 0.5f;
			}
			break;
		case Demolition:
			switch (_gen_count2) {
				case 0: // noise
					for (pframes_t i = 0 ; i < n_samples; ++i) {
						_buffer[i] = randf();
					}
					break;
				default:
					for (pframes_t i = 0 ; i < n_samples; ++i) {
						_buffer[i] = _demolition [_gen_count2];
					}
					break;
			}
			_gen_offset += n_samples;
			if (_gen_offset > _gen_period) {
				_gen_offset = 0;
				_gen_count2 = (_gen_count2 + 1) % (sizeof (_demolition) / sizeof (float));
			}
			break;
		case SquareWave:
			assert(_gen_period > 0);
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				if (_gen_offset < _gen_period * .5f) {
					_buffer[i] =  .40709f; // -6dBFS
				} else {
					_buffer[i] = -.40709f;
				}
				_gen_offset = (_gen_offset + 1) % _gen_period;
			}
			break;
		case KronekerDelta:
			assert(_gen_period > 0);
			memset (_buffer, 0, n_samples * sizeof (Sample));
			for (pframes_t i = 0; i < n_samples; ++i) {
				if (_gen_offset == 0) {
					_buffer[i] = 1.0f;
				}
				_gen_offset = (_gen_offset + 1) % _gen_period;
			}
			break;
		case OneHz:
			memset (_buffer, 0, n_samples * sizeof (Sample));
			{
				pframes_t pp = pulse_position ();
				/* MIDI Pulse needs 2 samples: Note on + off */
				if (pp < n_samples - 1) {
					_buffer[pp] = 1.0f;
					_buffer[pp + 1] = -1.0f;
				}
			}
			break;
		case SineSweepSwell:
		case SquareSweepSwell:
			assert(_wavetable && _gen_period > 0);
			{
				const float vols = 2.f / (float)_gen_perio2;
				for (pframes_t i = 0; i < n_samples; ++i) {
					const float g = fabsf (_gen_count2 * vols - 1.f);
					_buffer[i] = g * _wavetable[_gen_offset];
					_gen_offset = (_gen_offset + 1) % _gen_period;
					_gen_count2 = (_gen_count2 + 1) % _gen_perio2;
				}
			}
			break;
		case Loopback:
			memcpy((void*)_buffer, (void*)_wavetable, n_samples * sizeof(Sample));
			break;
		case SineWave:
		case SineWaveOctaves:
		case SineSweep:
		case SquareSweep:
			assert(_wavetable && _gen_period > 0);
			{
				pframes_t written = 0;
				while (written < n_samples) {
					const uint32_t remain = n_samples - written;
					const uint32_t to_copy = std::min(remain, _gen_period - _gen_offset);
					memcpy((void*)&_buffer[written],
							(void*)&_wavetable[_gen_offset],
							to_copy * sizeof(Sample));
					written += to_copy;
					_gen_offset = (_gen_offset + to_copy) % _gen_period;
				}
			}
			break;
		case UniformWhiteNoise:
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				_buffer[i] = .158489f * randf();
			}
			break;
		case GaussianWhiteNoise:
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				_buffer[i] = .089125f * grandf();
			}
			break;
		case PinkNoise:
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				// Paul Kellet's refined method
				// http://www.musicdsp.org/files/pink.txt
				// NB. If 'white' consists of uniform random numbers,
				// the pink noise will have an almost gaussian distribution.
				const float white = .0498f * randf ();
				_b0 = .99886f * _b0 + white * .0555179f;
				_b1 = .99332f * _b1 + white * .0750759f;
				_b2 = .96900f * _b2 + white * .1538520f;
				_b3 = .86650f * _b3 + white * .3104856f;
				_b4 = .55000f * _b4 + white * .5329522f;
				_b5 = -.7616f * _b5 - white * .0168980f;
				_buffer[i] = _b0 + _b1 + _b2 + _b3 + _b4 + _b5 + _b6 + white * 0.5362f;
				_b6 = white * 0.115926f;
			}
			break;
		case PonyNoise:
			for (pframes_t i = 0 ; i < n_samples; ++i) {
				const float white = 0.0498f * randf ();
				// Paul Kellet's economy method
				// http://www.musicdsp.org/files/pink.txt
				_b0 = 0.99765f * _b0 + white * 0.0990460f;
				_b1 = 0.96300f * _b1 + white * 0.2965164f;
				_b2 = 0.57000f * _b2 + white * 1.0526913f;
				_buffer[i] = _b0 + _b1 + _b2 + white * 0.1848f;
			}
			break;
		case LTC:
			while (_ltcbuf->read_space () < n_samples) {
				// we should pre-allocate (or add a zero-copy libltc API), whatever.
				ltcsnd_sample_t* enc_buf = (ltcsnd_sample_t*) malloc (ltc_encoder_get_buffersize (_ltc) * sizeof (ltcsnd_sample_t));
				for (int byteCnt = 0; byteCnt < 10; byteCnt++) {
					if (_ltc_rand != 0.f) {
						_ltc_spd += randf () * _ltc_rand;
						_ltc_spd = std::min (1.5f, std::max (0.5f, _ltc_spd));
					}
					ltc_encoder_encode_byte (_ltc, byteCnt, _ltc_spd);
					const int len = ltc_encoder_get_buffer (_ltc, enc_buf);
					for (int i = 0; i < len; ++i) {
						const float v1 = enc_buf[i] - 128;
						Sample v = v1 * 0.002;
						_ltcbuf->write (&v, 1);
					}
				}
				ltc_encoder_inc_timecode (_ltc);
				free (enc_buf);
			}
			_ltcbuf->read (_buffer, n_samples);
			break;
	}
	_gen_cycle = true;
}

void*
DummyAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		const std::set<BackendPortPtr>& connections = get_connections ();
		std::set<BackendPortPtr>::const_iterator it = connections.begin ();
		if (it == connections.end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			boost::shared_ptr<DummyAudioPort> source = boost::dynamic_pointer_cast<DummyAudioPort>(*it);
			assert (source && source->is_output ());
			if (source->is_physical() && source->is_terminal()) {
				source->get_buffer(n_samples); // generate signal.
			}
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != connections.end ()) {
				source = boost::dynamic_pointer_cast<DummyAudioPort>(*it);
				assert (source && source->is_output ());
				Sample* dst = buffer ();
				if (source->is_physical() && source->is_terminal()) {
					source->get_buffer(n_samples); // generate signal.
				}
				const Sample* src = source->const_buffer ();
				for (uint32_t s = 0; s < n_samples; ++s, ++dst, ++src) {
					*dst += *src;
				}
			}
		}
	} else if (is_output () && is_physical () && is_terminal()) {
		if (!_gen_cycle) {
			generate(n_samples);
		}
	}
	return _buffer;
}


DummyMidiPort::DummyMidiPort (DummyAudioBackend &b, const std::string& name, PortFlags flags)
	: DummyPort (b, name, flags)
	, _midi_seq_spb (0)
	, _midi_seq_time (0)
	, _midi_seq_pos (0)
	, _midi_seq_dat (0)
{
	_buffer.clear ();
	_loopback.clear ();
}

DummyMidiPort::~DummyMidiPort () {
	_buffer.clear ();
	_loopback.clear ();
}

struct MidiEventSorter {
	bool operator() (const boost::shared_ptr<DummyMidiEvent>& a, const boost::shared_ptr<DummyMidiEvent>& b) {
		return *a < *b;
	}
};

void DummyMidiPort::set_loopback (DummyMidiBuffer const * const src)
{
	_loopback.clear ();
	for (DummyMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
		_loopback.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
	}
}

std::string
DummyMidiPort::setup_generator (int seq_id, const float sr)
{
	DummyPort::setup_random_number_generator();
	if (seq_id < 0) {
		_midi_seq_spb = sr;
		return "One Hz";
	}
	_midi_seq_dat = DummyMidiData::sequences[seq_id % NUM_MIDI_EVENT_GENERATORS];
	_midi_seq_spb = sr * .5f; // 120 BPM, beat_time 1.0 per beat.
	_midi_seq_pos = 0;
	_midi_seq_time = 0;

	if (_midi_seq_dat && _midi_seq_dat[0].beat_time < -1) {
		_midi_seq_spb = sr / 25; // 25fps MTC
	} else if (_midi_seq_dat && _midi_seq_dat[0].beat_time < 0) {
		/* MIDI Clock 120 BPM */
		const double bpm = 120;
		double quarter_notes_per_beat = 1.0;

		const double samples_per_beat = sr * 60.0 / bpm;
		const double samples_per_quarter_note = samples_per_beat / quarter_notes_per_beat;
		const double clock_tick_interval = samples_per_quarter_note / 24.0;

		_midi_seq_spb = clock_tick_interval;
	}

	return DummyMidiData::sequence_names[seq_id];
}

void DummyMidiPort::midi_generate (const pframes_t n_samples)
{
	Glib::Threads::Mutex::Lock lm (generator_lock);
	if (_gen_cycle) {
		return;
	}

	_buffer.clear ();
	_gen_cycle = true;

	if (_midi_seq_spb != 0 && !_midi_seq_dat) {
		/* 1 Hz Note Events */
		pframes_t pp = pulse_position ();
		if (pp < n_samples - 1) {
			uint8_t md[3] = {0x90, 0x3c, 0x7f};
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (pp, md, 3)));
			md[0] = 0x80;
			md[2] = 0;
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (pp + 1, md, 3)));
		}
		return;
	}

	if (_midi_seq_spb == 0 || !_midi_seq_dat) {
		for (DummyMidiBuffer::const_iterator it = _loopback.begin (); it != _loopback.end (); ++it) {
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
		}
		return;
	}

	if (_midi_seq_dat[0].beat_time < -1) {
		/* MTC generator */
		const int audio_samples_per_video_frame = _midi_seq_spb; // sample-rate / 25
		const int audio_samples_per_qf =  audio_samples_per_video_frame / 4;

		samplepos_t tc_frame = _midi_seq_time / audio_samples_per_video_frame;
		samplepos_t tc_sample = tc_frame * audio_samples_per_video_frame;
		int qf = (tc_frame & 1) ? 4 : 0;
		while (tc_sample < _midi_seq_time + n_samples) {
			if (tc_sample >= _midi_seq_time) {
				uint8_t buf[2];
				buf[0] = 0xf1;

				int frame  =    tc_frame % 25;
				int second =   (tc_frame / 25) % 60;
				int minute =  ((tc_frame / 25) / 60) % 60;
				int hour   = (((tc_frame / 25) / 60) / 60);

				switch(qf & 7) {
					case 0: buf[1] =  0x00 |  (frame  & 0x0f); break;
					case 1: buf[1] =  0x10 | ((frame  & 0xf0) >> 4); break;
					case 2: buf[1] =  0x20 |  (second & 0x0f); break;
					case 3: buf[1] =  0x30 | ((second & 0xf0) >> 4); break;
					case 4: buf[1] =  0x40 |  (minute & 0x0f); break;
					case 5: buf[1] =  0x50 | ((minute & 0xf0) >> 4); break;
					case 6: buf[1] =  0x60 |  ((/* 25fps*/ 0x20 | hour) & 0x0f); break;
					case 7: buf[1] =  0x70 | (((/* 25fps*/ 0x20 | hour) & 0xf0)>>4); break;
				}
				_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (tc_sample - _midi_seq_time, buf, 2)));
			}
			tc_sample += audio_samples_per_qf;
			if (++qf == 8) {
				++tc_frame;
				qf = 0;
			}
		}

		_midi_seq_time += n_samples;
		if (_midi_seq_time >= /* 24 * 3600 * 25 */ 2160000LL * audio_samples_per_video_frame) {
			_midi_seq_time -= 2160000LL * audio_samples_per_video_frame; // 24h @ 25fps
		}

		return;

	} else if (_midi_seq_dat[0].beat_time < 0) {
		/* MClk generator */
		uint8_t buf[3];

		if (_midi_seq_time == 0) {
			/* Position Message */
			int64_t bcnt = 0; // beat count
			buf[0] = 0xf2;
			buf[1] = bcnt & 0x7f; // LSB
			buf[2] = (bcnt >> 7) & 0x7f; // MSB
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (0, buf, 3)));
		}

		/* MIDI System Real-Time Messages */
#define MIDI_RT_CLOCK    (0xF8)
#define MIDI_RT_START    (0xFA)
#define MIDI_RT_CONTINUE (0xFB)
#define MIDI_RT_STOP     (0xFC)

		if (_midi_seq_time == 0) {
			/* start */
			buf[0] = MIDI_RT_START;
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (0, buf, 1)));
		}

		const int clock_tick_interval = _midi_seq_spb; // samples per clock-tick
		samplepos_t clk_tick = _midi_seq_time / clock_tick_interval;
		samplepos_t clk_sample = clk_tick * clock_tick_interval;

		while (clk_sample < _midi_seq_time + n_samples) {
			if (clk_sample >= _midi_seq_time) {
				buf[0] = MIDI_RT_CLOCK;
				_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (clk_sample - _midi_seq_time, buf, 1)));
			}
			clk_sample += clock_tick_interval;
		}

		_midi_seq_time += n_samples;

		if (_midi_seq_time >= 16384 * 24 * clock_tick_interval) {
			_midi_seq_time -= 16384 * 24 * clock_tick_interval;
		}
		return;
	}

	while (1) {
		const int32_t ev_beat_time = _midi_seq_dat[_midi_seq_pos].beat_time * _midi_seq_spb - _midi_seq_time;
		if (ev_beat_time < 0) {
			break;
		}
		if ((pframes_t) ev_beat_time >= n_samples) {
			break;
		}
		_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (
						ev_beat_time,
						_midi_seq_dat[_midi_seq_pos].event,
						_midi_seq_dat[_midi_seq_pos].size
						)));
		++_midi_seq_pos;

		if (_midi_seq_dat[_midi_seq_pos].event[0] == 0xff && _midi_seq_dat[_midi_seq_pos].event[1] == 0xff) {
			_midi_seq_time -= _midi_seq_dat[_midi_seq_pos].beat_time * _midi_seq_spb;
			_midi_seq_pos = 0;
		}
	}
	_midi_seq_time += n_samples;
}


void* DummyMidiPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		_buffer.clear ();
		const std::set<BackendPortPtr>& connections = get_connections ();
		for (std::set<BackendPortPtr>::const_iterator i = connections.begin ();
				i != connections.end ();
				++i) {
			boost::shared_ptr<DummyMidiPort> source = boost::dynamic_pointer_cast<DummyMidiPort>(*i);
			if (source->is_physical() && source->is_terminal()) {
				source->get_buffer(n_samples); // generate signal.
			}
			const DummyMidiBuffer *src = source->const_buffer ();
			for (DummyMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
			}
		}
		std::stable_sort (_buffer.begin (), _buffer.end (), MidiEventSorter());
	} else if (is_output () && is_physical () && is_terminal()) {
		if (!_gen_cycle) {
			midi_generate(n_samples);
		}
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
	if (other.size () && other.data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.data (), other.size ());
	}
};

DummyMidiEvent::~DummyMidiEvent () {
	free (_data);
};
