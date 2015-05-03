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
#include <stdlib.h>

#include <glibmm.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#include "dummy_audiobackend.h"
#include "dummy_midi_seq.h"

#include "pbd/error.h"
#include "ardour/port_manager.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
size_t DummyAudioBackend::_max_buffer_size = 8192;
std::vector<std::string> DummyAudioBackend::_midi_options;
std::vector<AudioBackend::DeviceStatus> DummyAudioBackend::_device_status;

#ifdef PLATFORM_WINDOWS
static double _win_pc_rate = 0; // usec per tick
#endif

static int64_t _x_get_monotonic_usec() {
#ifdef PLATFORM_WINDOWS
	if (_win_pc_rate > 0) {
		LARGE_INTEGER Count;
		// not very reliable, but the only realistic way for sub milli-seconds
		if (QueryPerformanceCounter (&Count)) {
			return (int64_t) (Count.QuadPart * _win_pc_rate);
		}
		return -1;
	}
#endif
	return g_get_monotonic_time();
}

DummyAudioBackend::DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, _running (false)
	, _freewheel (false)
	, _freewheeling (false)
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
	, _port_change_flag (false)
{
	_instance_name = s_instance_name;
	_device = _("Silence");
	pthread_mutex_init (&_port_callback_mutex, 0);
}

DummyAudioBackend::~DummyAudioBackend ()
{
	pthread_mutex_destroy (&_port_callback_mutex);
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
	if (_device_status.empty()) {
		_device_status.push_back (DeviceStatus (_("Silence"), true));
		_device_status.push_back (DeviceStatus (_("Sine Wave"), true));
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
	return true;
}

bool
DummyAudioBackend::can_change_buffer_size_when_running () const
{
	return true;
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
	if (bs <= 0 || bs >= _max_buffer_size) {
		return -1;
	}
	_samples_per_period = bs;

	/* update port latencies
	 * with 'Loopback' there is exactly once cycle latency,
	 * divide it between In + Out;
	 */
	LatencyRange lr;
	lr.min = lr.max = _systemic_input_latency;
	for (std::vector<DummyAudioPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
		set_latency_range (*it, false, lr);
	}
	for (std::vector<DummyMidiPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
		set_latency_range (*it, false, lr);
	}

	lr.min = lr.max = _systemic_output_latency;
	for (std::vector<DummyAudioPort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		set_latency_range (*it, true, lr);
	}
	for (std::vector<DummyMidiPort*>::const_iterator it = _system_midi_out.begin (); it != _system_midi_out.end (); ++it) {
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
		return -1;
	}

	if (_ports.size()) {
		PBD::warning << _("DummyAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		for (std::vector<DummyPort*>::const_iterator it = _ports.begin (); it != _ports.end (); ++it) {
			PBD::info << _("DummyAudioBackend: port '") << (*it)->name () << "' exists." << endmsg;
		}
		_system_inputs.clear();
		_system_outputs.clear();
		_system_midi_in.clear();
		_system_midi_out.clear();
		_ports.clear();
	}

	if (register_system_ports()) {
		PBD::error << _("DummyAudioBackend: failed to register system ports.") << endmsg;
		return -1;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("DummyAudioBackend: Could not re-establish ports.") << endmsg;
		stop ();
		return -1;
	}

	engine.reconnect_ports ();
	_port_change_flag = false;

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
framepos_t
DummyAudioBackend::sample_time ()
{
	return _processed_samples;
}

framepos_t
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
		pthread_attr_destroy (&attr);
		return -1;
	}
	pthread_attr_destroy (&attr);

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
	if (!_running) {
		PBD::info << _("DummyBackend::register_port: Engine is not running.") << endmsg;
	}
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
			port = new DummyAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new DummyMidiPort (*this, name, flags);
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
	if (!_running) {
		PBD::info << _("DummyBackend::unregister_port: Engine is not running.") << endmsg;
		assert (!valid_port (port_handle));
		return;
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
	} else if (_device == _("Loopback")) {
		gt = DummyAudioPort::Loopback;
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
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		_system_inputs.push_back (static_cast<DummyAudioPort*>(p));
		static_cast<DummyAudioPort*>(p)->setup_generator (gt, _samplerate);
	}

	lr.min = lr.max = _systemic_output_latency;
	for (int i = 1; i <= a_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::AUDIO, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_outputs.push_back (static_cast<DummyAudioPort*>(p));
	}

	/* midi ports */
	lr.min = lr.max = _systemic_input_latency;
	for (int i = 0; i < m_ins; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_capture_%d", i+1);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsOutput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, false, lr);
		_system_midi_in.push_back (static_cast<DummyMidiPort*>(p));
		if (_midi_mode == MidiGenerator) {
			static_cast<DummyMidiPort*>(p)->setup_generator (i % NUM_MIDI_EVENT_GENERATORS, _samplerate);
		}
	}

	lr.min = lr.max = _systemic_output_latency;
	for (int i = 1; i <= m_out; ++i) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "system:midi_playback_%d", i);
		PortHandle p = add_port(std::string(tmp), DataType::MIDI, static_cast<PortFlags>(IsInput | IsPhysical | IsTerminal));
		if (!p) return -1;
		set_latency_range (p, true, lr);
		_system_midi_out.push_back (static_cast<DummyMidiPort*>(p));
	}
	return 0;
}

void
DummyAudioBackend::unregister_ports (bool system_only)
{
	_system_inputs.clear();
	_system_outputs.clear();
	_system_midi_in.clear();
	_system_midi_out.clear();

	for (std::vector<DummyPort*>::iterator i = _ports.begin (); i != _ports.end ();) {
		DummyPort* port = *i;
		if (! system_only || (port->is_physical () && port->is_terminal ())) {
			port->disconnect_all ();
			delete port;
			i = _ports.erase (i);
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
		// nevermind, ::get_buffer() sorts events, but always print warning
		fprintf (stderr, "DummyMidiBuffer: it's too late for this event.\n");
	}
	dst.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (timestamp, buffer, size)));
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
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::error << _("DummyPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}
	DummyPort *p =  static_cast<DummyPort*>(port);
	assert(p);

	r = p->latency_range (for_playback);
	if (p->is_physical() && p->is_terminal()) {
		if (p->is_input() && for_playback) {
			const size_t l_in = _samples_per_period * .25;
			r.min += l_in;
			r.max += l_in;
		}
		if (p->is_output() && !for_playback) {
			/* with 'Loopback' there is exactly once cycle latency, divide it between In + Out; */
			const size_t l_in = _samples_per_period * .25;
			const size_t l_out = _samples_per_period - l_in;
			r.min += l_out;
			r.max += l_out;
		}
	}
	return r;
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
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
DummyAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (size_t i = 0; i < _ports.size (); ++i) {
		DummyPort* port = _ports[i];
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
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
	assert (port);
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

	manager.registration_callback();
	manager.graph_order_callback();

	int64_t clock1, clock2;
	clock1 = -1;
	while (_running) {

		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);
		}

		// re-set input buffers, generate on demand.
		for (std::vector<DummyAudioPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it) {
			(*it)->next_period();
		}
		for (std::vector<DummyMidiPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it) {
			(*it)->next_period();
		}

		if (engine.process_callback (_samples_per_period)) {
			return 0;
		}
		_processed_samples += _samples_per_period;

		if (_device == _("Loopback") && _midi_mode != MidiToAudio) {
			int opn = 0;
			int opc = _system_outputs.size();
			for (std::vector<DummyAudioPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++opn) {
				DummyAudioPort* op = _system_outputs[(opn % opc)];
				(*it)->fill_wavetable ((const float*)op->get_buffer (_samples_per_period), _samples_per_period);
			}
		}

		if (_midi_mode == MidiLoopback) {
			int opn = 0;
			int opc = _system_midi_out.size();
			for (std::vector<DummyMidiPort*>::const_iterator it = _system_midi_in.begin (); it != _system_midi_in.end (); ++it, ++opn) {
				DummyMidiPort* op = _system_midi_out[(opn % opc)];
				op->get_buffer(0); // mix-down
				(*it)->set_loopback (op->const_buffer());
			}
		}
		else if (_midi_mode == MidiToAudio) {
			int opn = 0;
			int opc = _system_midi_out.size();
			for (std::vector<DummyAudioPort*>::const_iterator it = _system_inputs.begin (); it != _system_inputs.end (); ++it, ++opn) {
				DummyMidiPort* op = _system_midi_out[(opn % opc)];
				op->get_buffer(0); // mix-down
				(*it)->midi_to_wavetable (op->const_buffer(), _samples_per_period);
			}
		}

		if (!_freewheel) {
			const int64_t nominal_time = 1e6 * _samples_per_period / _samplerate;
			clock2 = _x_get_monotonic_usec();
			bool timers_ok = true;

			/* querying the performance counter can fail occasionally (-1).
			 * Also on some multi-core systems, timers are CPU specific and not
			 * synchronized. We assume they differ more than a few milliseconds
			 * (4 * nominal cycle time) and simply ignore cases where the
			 * execution switches cores.
			 */
			if (clock1 < 0 || clock2 < 0 || (clock1 > clock2) || (clock2 - clock1) > 4 * nominal_time) {
				clock1 = 0;
				clock2 = nominal_time;
				timers_ok = false;
			}

			const int64_t elapsed_time = clock2 - clock1;

			if (timers_ok)
			{ // low pass filter
				const float load = elapsed_time / (float) nominal_time;
				if (load > _dsp_load) {
					_dsp_load = load;
				} else {
					const float a = .2 * _samples_per_period / _samplerate;
					_dsp_load = _dsp_load + a * (load - _dsp_load) + 1e-12;
				}
			}

			if (elapsed_time < nominal_time) {
				Glib::usleep (nominal_time - elapsed_time);
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
	"Dummy",
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
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER Frequency;
	if (!QueryPerformanceFrequency(&Frequency) || Frequency.QuadPart < 1) {
		_win_pc_rate = 0;
	} else {
		_win_pc_rate = 1000000.0 / Frequency.QuadPart;
	}
#endif
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
	if (_instance) {
		return _instance->is_running();
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
	: _dummy_backend (b)
	, _name  (name)
	, _flags (flags)
	, _rseed (0)
	, _gen_cycle (false)
{
	_capture_latency_range.min = 0;
	_capture_latency_range.max = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
	_dummy_backend.port_connect_add_remove_callback();
}

DummyPort::~DummyPort () {
	disconnect_all ();
	_dummy_backend.port_connect_add_remove_callback();
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
		_dummy_backend.port_connect_callback (name(),  port->name(), true);
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
		_dummy_backend.port_connect_callback (name(),  port->name(), false);
	}
}


void DummyPort::disconnect_all ()
{
	while (!_connections.empty ()) {
		_connections.back ()->_disconnect (this, false);
		_dummy_backend.port_connect_callback (name(),  _connections.back ()->name(), false);
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

void DummyPort::setup_random_number_generator ()
{
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER Count;
	if (QueryPerformanceCounter (&Count)) {
		_rseed = Count.QuadPart % UINT_MAX;
	} else
#endif
	{
	_rseed = g_get_monotonic_time() % UINT_MAX;
	}
	_rseed = (_rseed + (uint64_t)this) % UINT_MAX;
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
{
	memset (_buffer, 0, sizeof (_buffer));
}

DummyAudioPort::~DummyAudioPort () {
	free(_wavetable);
	_wavetable = 0;
}

void DummyAudioPort::setup_generator (GeneratorType const g, float const samplerate)
{
	DummyPort::setup_random_number_generator();
	_gen_type = g;

	switch (_gen_type) {
		case PinkNoise:
		case PonyNoise:
		case UniformWhiteNoise:
		case GaussianWhiteNoise:
		case Silence:
			break;
		case KronekerDelta:
			_gen_period = (5 + randi() % (int)(samplerate / 20.f));
			break;
		case SquareWave:
			_gen_period = (5 + randi() % (int)(samplerate / 20.f)) & ~1;
			break;
		case SineWave:
			_gen_period = 5 + randi() % (int)(samplerate / 20.f);
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
		case Loopback:
			_wavetable = (Sample*) malloc (DummyAudioBackend::max_buffer_size() * sizeof(Sample));
			break;
	}
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
			const unsigned char *d = (*it)->const_data();
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
		case SineSweepSwell:
		case SquareSweepSwell:
			assert(_wavetable && _gen_period > 0);
			{
				const float vols = 2.f / (float)_gen_perio2;
				for (pframes_t i = 0; i < n_samples; ++i) {
					const float g = fabsf (_gen_count2 * vols - 1.0);
					_buffer[i] = g * _wavetable[_gen_offset];
					_gen_offset = (_gen_offset + 1) % _gen_period;
					_gen_count2 = (_gen_count2 + 1) % _gen_perio2;
				}
			}
			break;
		case Loopback:
			_gen_period = n_samples; // XXX DummyBackend::_samples_per_period;
		case SineWave:
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
	}
	_gen_cycle = true;
}

void* DummyAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		std::vector<DummyPort*>::const_iterator it = get_connections ().begin ();
		if (it == get_connections ().end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			DummyAudioPort * source = static_cast<DummyAudioPort*>(*it);
			assert (source && source->is_output ());
			if (source->is_physical() && source->is_terminal()) {
				source->get_buffer(n_samples); // generate signal.
			}
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != get_connections ().end ()) {
				source = static_cast<DummyAudioPort*>(*it);
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

void DummyMidiPort::setup_generator (int seq_id, const float sr)
{
	DummyPort::setup_random_number_generator();
	_midi_seq_dat = DummyMidiData::sequences[seq_id % NUM_MIDI_EVENT_GENERATORS];
	_midi_seq_spb = sr * .5f; // 120 BPM, beat_time 1.0 per beat.
	_midi_seq_pos = 0;
	_midi_seq_time = 0;
}

void DummyMidiPort::midi_generate (const pframes_t n_samples)
{
	Glib::Threads::Mutex::Lock lm (generator_lock);
	if (_gen_cycle) {
		return;
	}

	_buffer.clear ();
	_gen_cycle = true;

	if (_midi_seq_spb == 0 || !_midi_seq_dat) {
		for (DummyMidiBuffer::const_iterator it = _loopback.begin (); it != _loopback.end (); ++it) {
			_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
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
		for (std::vector<DummyPort*>::const_iterator i = get_connections ().begin ();
				i != get_connections ().end ();
				++i) {
			DummyMidiPort * source = static_cast<DummyMidiPort*>(*i);
			if (source->is_physical() && source->is_terminal()) {
				source->get_buffer(n_samples); // generate signal.
			}
			const DummyMidiBuffer *src = source->const_buffer ();
			for (DummyMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				_buffer.push_back (boost::shared_ptr<DummyMidiEvent>(new DummyMidiEvent (**it)));
			}
		}
		std::sort (_buffer.begin (), _buffer.end (), MidiEventSorter());
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
	if (other.size () && other.const_data ()) {
		_data = (uint8_t*) malloc (other.size ());
		memcpy (_data, other.const_data (), other.size ());
	}
};

DummyMidiEvent::~DummyMidiEvent () {
	free (_data);
};
