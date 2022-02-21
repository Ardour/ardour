/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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
#include <regex.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"

#include "ardour/port_manager.h"

#include "ndi_backend.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;

const size_t NDIBackend::_max_buffer_size = 8192;

#define N_CHANNELS (2)

NDIBackend::NDIBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, PortEngineSharedImpl (e, s_instance_name)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _freewheeling (false)
	, _last_process_start (0)
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _systemic_audio_input_latency (0)
	, _systemic_audio_output_latency (0)
	, _dsp_load (0)
	, _processed_samples (0)
{
	_instance_name = s_instance_name;
}

NDIBackend::~NDIBackend ()
{
	clear_ports ();
}

/* AUDIOBACKEND API */

std::string
NDIBackend::name () const
{
	return X_("NDI");
}

bool
NDIBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
NDIBackend::enumerate_devices () const
{
	std::vector<AudioBackend::DeviceStatus> devices;
	devices.push_back (DeviceStatus (_("Default Playback"), true));
	return devices;
}

std::vector<float>
NDIBackend::available_sample_rates (const std::string&) const
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
NDIBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
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
NDIBackend::available_input_channel_count (const std::string&) const
{
	return 0;
}

uint32_t
NDIBackend::available_output_channel_count (const std::string&) const
{
	return N_CHANNELS;
}

bool
NDIBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
NDIBackend::can_change_buffer_size_when_running () const
{
	return false;
}

int
NDIBackend::set_device_name (const std::string& d)
{
	return 0;
}

int
NDIBackend::set_sample_rate (float sr)
{
	if (sr <= 0) {
		return -1;
	}
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
NDIBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs > _max_buffer_size) {
		return -1;
	}

	_samples_per_period = bs;

	engine.buffer_size_change (bs);
	return 0;
}

int
NDIBackend::set_interleaved (bool yn)
{
	if (!yn) {
		return 0;
	}
	return -1;
}

int
NDIBackend::set_input_channels (uint32_t cc)
{
	return 0;
}

int
NDIBackend::set_output_channels (uint32_t cc)
{
	return 0;
}

int
NDIBackend::set_systemic_input_latency (uint32_t sl)
{
	return 0; // XXX
}

int
NDIBackend::set_systemic_output_latency (uint32_t sl)
{
	return 0; // XXX
}

/* Retrieving parameters */
std::string
NDIBackend::device_name () const
{
	return _("Default Playback");
}

float
NDIBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
NDIBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
NDIBackend::interleaved () const
{
	return false;
}

uint32_t
NDIBackend::input_channels () const
{
	return N_CHANNELS;
}

uint32_t
NDIBackend::output_channels () const
{
	return N_CHANNELS;
}

uint32_t
NDIBackend::systemic_input_latency () const
{
	return _systemic_audio_input_latency;
}

uint32_t
NDIBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

/* MIDI */
std::vector<std::string>
NDIBackend::enumerate_midi_options () const
{
	std::vector<std::string> midi_options;
	midi_options.push_back (get_standard_device_name (DeviceNone));
	return midi_options;
}

std::vector<AudioBackend::DeviceStatus>
NDIBackend::enumerate_midi_devices () const
{
	return std::vector<AudioBackend::DeviceStatus> ();
}

int
NDIBackend::set_midi_option (const std::string& opt)
{
	return 0;
}

std::string
NDIBackend::midi_option () const
{
	return get_standard_device_name (DeviceNone);
}

/* External control app */
std::string
NDIBackend::control_app_name () const
{
	return "";
}

void
NDIBackend::launch_control_app ()
{
}

/* State Control */

static void*
pthread_process (void* arg)
{
	NDIBackend* d = static_cast<NDIBackend*> (arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
NDIBackend::_start (bool /*for_latency_measurement*/)
{
	if (!_active && _run) {
		PBD::error << _("NDIBackend: already active.") << endmsg;
		/* recover from 'halted', reap threads */
		stop ();
	}

	if (_active || _run) {
		PBD::info << _("NDIBackend: already active.") << endmsg;
		return BackendReinitializationError;
	}

	clear_ports ();

	/* reset internal state */
	_dsp_load                      = 0;
	_freewheeling                  = false;
	_freewheel                     = false;
	_last_process_start            = 0;
	_systemic_audio_input_latency  = 0;
	_systemic_audio_output_latency = 0;

	/* TODO connect to NDI-server and prepare stream */

	/* register ports, notify port-engine */
	if (register_system_ports ()) {
		PBD::error << _("NDIBackend: failed to register system ports.") << endmsg;
		// close_ndi (); // XXX
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("NDIBackend: Could not re-establish ports.") << endmsg;
		// close_ndi (); // XXX
		return PortReconnectError;
	}

	engine.reconnect_ports ();

	_run = true;
	g_atomic_int_set (&_port_change_flag, 0);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MAIN, PBD_RT_STACKSIZE_PROC,
	                                 &_main_thread, pthread_process, this)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &_main_thread, pthread_process, this)) {
			PBD::error << _("NDIBackend: failed to create process thread.") << endmsg;
			stop ();
			_run = false;
			return ProcessThreadStartError;
		} else {
			PBD::warning << _("NDIBackend: cannot acquire realtime permissions.") << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) {
		Glib::usleep (1000);
	}

	if (timeout == 0 || !_active) {
		PBD::error << _("NDIBackend: failed to start process thread.") << endmsg;
		_run = false;
		// close_ndi (); // XXX
		return ProcessThreadStartError;
	}

	return NoError;
}

int
NDIBackend::stop ()
{
	void* status;
	if (!_run) {
		return 0;
	}

	_run = false;
	/* TODO: STOP NDI */

	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("NDIBackend: failed to terminate.") << endmsg;
		return -1;
	}
	unregister_ports ();

	/* TODO: close NDI connection */

	return (_active == false) ? 0 : -1;
}

int
NDIBackend::freewheel (bool onoff)
{
	_freewheeling = onoff;
	return 0;
}

float
NDIBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
NDIBackend::raw_buffer_size (DataType t)
{
	switch (t) {
		case DataType::AUDIO:
			return _samples_per_period * sizeof (Sample);
		case DataType::MIDI:
			return _max_buffer_size;
	}
	return 0;
}

/* Process time */
samplepos_t
NDIBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
NDIBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
NDIBackend::samples_since_cycle_start ()
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
NDIBackend::ndi_process_thread (void* arg)
{
	ThreadData*              td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void ()> f  = td->f;
	delete td;
	f ();
	return 0;
}

int
NDIBackend::create_process_thread (boost::function<void ()> func)
{
	pthread_t   thread_id;
	ThreadData* td = new ThreadData (this, func, PBD_RT_STACKSIZE_PROC);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_PROC, PBD_RT_STACKSIZE_PROC,
	                                 &thread_id, ndi_process_thread, td)) {
		if (pbd_pthread_create (PBD_RT_STACKSIZE_PROC, &thread_id, ndi_process_thread, td)) {
			PBD::error << _("AudioEngine: cannot create process thread.") << endmsg;
			return -1;
		}
	}

	_threads.push_back (thread_id);
	return 0;
}

int
NDIBackend::join_process_threads ()
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
NDIBackend::in_process_thread ()
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
NDIBackend::process_thread_count ()
{
	return _threads.size ();
}

void
NDIBackend::update_latencies ()
{
	/* trigger latency callback in RT thread (locked graph) */
	port_connect_add_remove_callback ();
}

/* PORTENGINE API */

void*
NDIBackend::private_handle () const
{
	return NULL;
}

const std::string&
NDIBackend::my_name () const
{
	return _instance_name;
}

int
NDIBackend::register_system_ports ()
{
	LatencyRange lr;
	/* input/source ports */
	lr.min = lr.max = _systemic_audio_input_latency;
	for (int i = 1; i <= N_CHANNELS; ++i) {
		char tmp[64];
		snprintf (tmp, sizeof (tmp), "system:capture_%d", i);
		BackendPortPtr p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsOutput | IsPhysical | IsTerminal));
		if (!p) {
			return -1;
		}
		set_latency_range (p, true, lr);
		_system_inputs.push_back (p);
	}

	/* output/sink ports */
	lr.min = lr.max = _systemic_audio_output_latency;
	for (int i = 1; i <= N_CHANNELS; ++i) {
		char tmp[64];
		snprintf (tmp, sizeof (tmp), "system:playback_%d", i);
		BackendPortPtr p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsInput | IsPhysical | IsTerminal));
		if (!p) {
			return -1;
		}
		set_latency_range (p, true, lr);
		//p->set_hw_port_name ("")
		_system_outputs.push_back (p);
	}
	return 0;
}

BackendPort*
NDIBackend::port_factory (std::string const& name, ARDOUR::DataType type, ARDOUR::PortFlags flags)
{
	BackendPort* port = 0;

	switch (type) {
		case DataType::AUDIO:
			port = new NDIAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new NDIMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << string_compose (_("%1::register_port: Invalid Data Type."), _instance_name) << endmsg;
			return 0;
	}

	return port;
}

/* MIDI */
int
NDIBackend::midi_event_get (pframes_t& timestamp, size_t& size, uint8_t const** buf, void* port_buffer, uint32_t event_index)
{
	assert (buf && port_buffer);
	NDIMidiBuffer& source = *static_cast<NDIMidiBuffer*> (port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	NDIMidiEvent* const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size      = event->size ();
	*buf      = event->data ();
	return 0;
}

int
NDIBackend::midi_event_put (void*          port_buffer,
                            pframes_t      timestamp,
                            const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	NDIMidiBuffer& dst = *static_cast<NDIMidiBuffer*> (port_buffer);
	dst.push_back (boost::shared_ptr<NDIMidiEvent> (new NDIMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
NDIBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<NDIMidiBuffer*> (port_buffer)->size ();
}

void
NDIBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
	NDIMidiBuffer* buf = static_cast<NDIMidiBuffer*> (port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
NDIBackend::can_monitor_input () const
{
	return false;
}

int
NDIBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
NDIBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
NDIBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
NDIBackend::set_latency_range (PortEngine::PortHandle port_handle, bool for_playback, LatencyRange latency_range)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	if (!valid_port (port)) {
		PBD::error << _("NDIAudioPort::set_latency_range (): invalid port.") << endmsg;
	}
	port->set_latency_range (latency_range, for_playback);
}

LatencyRange
NDIBackend::get_latency_range (PortEngine::PortHandle port_handle, bool for_playback)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);
	LatencyRange   r;

	if (!valid_port (port)) {
		PBD::error << _("NDIAudioPort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}

	r = port->latency_range (for_playback);

	if (port->is_physical () && port->is_terminal ()) {
		if (port->is_input () && for_playback) {
			r.min += _samples_per_period + _systemic_audio_output_latency;
			r.max += _samples_per_period + _systemic_audio_output_latency;
		}
		if (port->is_output () && !for_playback) {
			r.min += _samples_per_period + _systemic_audio_input_latency;
			r.max += _samples_per_period + _systemic_audio_input_latency;
		}
	}

	return r;
}

/* Getting access to the data buffer for a port */

void*
NDIBackend::get_buffer (PortEngine::PortHandle port_handle, pframes_t nframes)
{
	BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (port_handle);

	assert (port);
	assert (valid_port (port));

	return port->get_buffer (nframes);
}

/* Engine Process */
void*
NDIBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active            = true;
	_processed_samples = 0;

	manager.registration_callback ();
	manager.graph_order_callback ();

	/* TODO begin NDI streaming */

	_dsp_load_calc.reset ();

	while (_run) {
		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);

			if (_freewheel) {
				/* TODO pause NDI stream */
			}

			/* TODO flush stream (before and after freewheeling) */

			if (!_freewheel) {
				/* TODO resume NDI stream */
				_dsp_load_calc.reset ();
			}
		}

		if (!_freewheel) {
			/* TODO: wait for NDI to provide data */

			int64_t clock1 = g_get_monotonic_time ();
			/* call engine process callback */
			_last_process_start = g_get_monotonic_time ();
			if (engine.process_callback (_samples_per_period)) {
				/* ERROR -- TODO: stop NDI stream */
				_active = false;
				return 0;
			}

			/* write back audio */
			uint32_t i = 0;
			float    buf[_max_buffer_size * N_CHANNELS];
			assert (_system_outputs.size () == N_CHANNELS);

			/* interleave */
			for (std::vector<BackendPortPtr>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
				BackendPortPtr port = boost::dynamic_pointer_cast<BackendPort> (*it);
				const float*   src  = (const float*)port->get_buffer (_samples_per_period);
				for (size_t n = 0; n < _samples_per_period; ++n) {
					buf[N_CHANNELS * n + i] = src[n];
				}
			}
			/* TODO write interlevaed data to NDI */

			_processed_samples += _samples_per_period;

			_dsp_load_calc.set_max_time (_samplerate, _samples_per_period);
			_dsp_load_calc.set_start_timestamp_us (clock1);
			_dsp_load_calc.set_stop_timestamp_us (g_get_monotonic_time ());
			_dsp_load = _dsp_load_calc.get_dsp_load ();

#if 1 // Sleep for now
			const int64_t elapsed_time = _dsp_load_calc.elapsed_time_us ();
			const int64_t nominal_time = _dsp_load_calc.get_max_time_us ();
			const int64_t sleepy = nominal_time - elapsed_time;
			Glib::usleep (std::max ((int64_t) 100, sleepy));
#endif
		} else {
			/* Freewheelin' */
			_last_process_start = 0;
			if (engine.process_callback (_samples_per_period)) {
				_active = false;
				return 0;
			}

			_dsp_load = 1.0f;
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
			update_system_port_latencies ();
			engine.latency_callback (false);
			engine.latency_callback (true);
		}
	}

	_active = false;
	if (_run) {
		engine.halted_callback ("NDI I/O error.");
	}
	return 0;
}

/******************************************************************************/

static boost::shared_ptr<NDIBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int  instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int  deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	_("NDI"),
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
		_instance.reset (new NDIBackend (e, _descriptor));
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

NDIAudioPort::NDIAudioPort (NDIBackend& b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock (_buffer, sizeof (_buffer));
}

NDIAudioPort::~NDIAudioPort ()
{
}

void*
NDIAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		const std::set<BackendPortPtr>&          connections = get_connections ();
		std::set<BackendPortPtr>::const_iterator it          = connections.begin ();

		if (it == connections.end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			boost::shared_ptr<NDIAudioPort> source = boost::dynamic_pointer_cast<NDIAudioPort> (*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != connections.end ()) {
				source = boost::dynamic_pointer_cast<NDIAudioPort> (*it);
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

NDIMidiPort::NDIMidiPort (NDIBackend& b, const std::string& name, PortFlags flags)
	: BackendPort (b, name, flags)
{
	_buffer.clear ();
	_buffer.reserve (256);
}

NDIMidiPort::~NDIMidiPort ()
{
}

struct MidiEventSorter {
	bool
	operator() (const boost::shared_ptr<NDIMidiEvent>& a, const boost::shared_ptr<NDIMidiEvent>& b)
	{
		return *a < *b;
	}
};

void* NDIMidiPort::get_buffer (pframes_t /*n_samples*/)
{
	if (is_input ()) {
		_buffer.clear ();
		const std::set<BackendPortPtr>& connections = get_connections ();
		for (std::set<BackendPortPtr>::const_iterator i = connections.begin ();
		     i != connections.end ();
		     ++i) {
			const NDIMidiBuffer* src = boost::dynamic_pointer_cast<NDIMidiPort> (*i)->const_buffer ();
			for (NDIMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				_buffer.push_back (*it);
			}
		}
		std::stable_sort (_buffer.begin (), _buffer.end (), MidiEventSorter ());
	}
	return &_buffer;
}

NDIMidiEvent::NDIMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
	: _size (size)
	, _timestamp (timestamp)
{
	if (size > 0 && size < MaxNDIMidiEventSize) {
		memcpy (_data, data, size);
	}
}

NDIMidiEvent::NDIMidiEvent (const NDIMidiEvent& other)
	: _size (other.size ())
	, _timestamp (other.timestamp ())
{
	if (other.size () && other.const_data ()) {
		assert (other._size < MaxNDIMidiEventSize);
		memcpy (_data, other._data, other._size);
	}
};
