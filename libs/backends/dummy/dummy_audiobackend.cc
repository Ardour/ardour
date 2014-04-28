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
#include "dummy_audiobackend.h"
#include "pbd/error.h"
#include "i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;
DummyAudioBackend::DummyAudioBackend (AudioEngine& e)
	: AudioBackend (e)
	, _running (false)
	, _freewheeling (false)
	, _samplerate (48000)
	, _audio_buffersize (1024)
	, _dsp_load (0)
	, _n_inputs (0)
	, _n_outputs (0)
	, _systemic_input_latency (0)
	, _systemic_output_latency (0)
	, _processed_samples (0)
{
	_instance_name = s_instance_name;
}

DummyAudioBackend::~DummyAudioBackend ()
{
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
	std::vector<AudioBackend::DeviceStatus> s;
	s.push_back (DeviceStatus (_("Dummy"), true));
	return s;
}

std::vector<float>
DummyAudioBackend::available_sample_rates (const std::string&) const
{
	std::vector<float> sr;
	sr.push_back (44100.0);
	sr.push_back (48000.0);
	return sr;
}

std::vector<uint32_t>
DummyAudioBackend::available_buffer_sizes (const std::string&) const
{
	std::vector<uint32_t> bs;
	bs.push_back (64);
	bs.push_back (1024);
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
DummyAudioBackend::set_device_name (const std::string&)
{
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
	return -1;
	_audio_buffersize = bs;
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
DummyAudioBackend::set_input_channels (uint32_t)
{
	return -1;
}

int
DummyAudioBackend::set_output_channels (uint32_t)
{
	return -1;
}

int
DummyAudioBackend::set_systemic_input_latency (uint32_t)
{
	return -1;
}

int
DummyAudioBackend::set_systemic_output_latency (uint32_t)
{
	return -1;
}

/* Retrieving parameters */
std::string
DummyAudioBackend::device_name () const
{
	return _("Dummy Device");
}

float
DummyAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
DummyAudioBackend::buffer_size () const
{
	return _audio_buffersize;
}

bool
DummyAudioBackend::interleaved () const
{
	return false;
}

uint32_t
DummyAudioBackend::input_channels () const
{
	return 0;
}

uint32_t
DummyAudioBackend::output_channels () const
{
	return 0;
}

uint32_t
DummyAudioBackend::systemic_input_latency () const
{
	return 0;
}

uint32_t
DummyAudioBackend::systemic_output_latency () const
{
	return 0;
}

/* MIDI */
std::vector<std::string>
DummyAudioBackend::enumerate_midi_options () const
{
	std::vector<std::string> m;
	m.push_back (_("None"));
	return m;
}

int
DummyAudioBackend::set_midi_option (const std::string&)
{
	return -1;
}

std::string
DummyAudioBackend::midi_option () const
{
	return "";
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
	if (pthread_create (&_main_thread, NULL, pthread_process, this)) {
		PBD::error << _("DummyAudioBackend: cannot start.") << endmsg;
	}

	int timeout = 5000;
	while (!_running && --timeout > 0) { usleep (1000); }

	if (timeout == 0 || !_running) {
		PBD::error << _("DummyAudioBackend: failed to start process thread.") << endmsg;
		return -1;
	}

	if (engine.reestablish_ports ()) {
		PBD::error << _("DummyAudioBackend: Could not re-establish ports.") << endmsg;
		stop ();
		return -1;
	}

	engine.reconnect_ports ();
	return 0;
}

int
DummyAudioBackend::stop ()
{
	void *status;
	if (!_running) {
		return -1;
	}

	_running = false;
	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("DummyAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}
	return 0;
}

int
DummyAudioBackend::freewheel (bool onoff)
{
	if (onoff != _freewheeling) {
		return 0;
	}
	_freewheeling = onoff;
	engine.freewheel_callback (onoff);
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
	return 0;
}

/* Process time */
pframes_t
DummyAudioBackend::sample_time ()
{
	return _processed_samples;
}

pframes_t
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
	for (std::vector<pthread_t>::const_iterator i = _threads.begin (); i != _threads.end (); ++i)
	{
#ifdef COMPILER_MINGW
		if (*i == GetCurrentThread ()) {
			return true;
		}
#else // pthreads
		if (pthread_equal (*i, pthread_self ()) != 0) {
			return true;
		}
#endif
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
DummyAudioBackend::set_port_name (PortEngine::PortHandle, const std::string&)
{
	return -1;
}

std::string
DummyAudioBackend::get_port_name (PortEngine::PortHandle) const
{
	return "port:XXX";
}

PortEngine::PortHandle
DummyAudioBackend::get_port_by_name (const std::string&) const
{
	PortEngine::PortHandle port_handle = 0;
	return port_handle;
}

int
DummyAudioBackend::get_ports (
		const std::string& port_name_pattern,
		DataType type, PortFlags flags,
		std::vector<std::string>&) const
{
	return 0;
}

DataType
DummyAudioBackend::port_data_type (PortEngine::PortHandle) const
{
	return DataType::AUDIO;
}

PortEngine::PortHandle
DummyAudioBackend::register_port (
		const std::string&,
		ARDOUR::DataType,
		ARDOUR::PortFlags)
{
	PortEngine::PortHandle port_handle = 0;
	return port_handle;
}

void
DummyAudioBackend::unregister_port (PortEngine::PortHandle)
{
}

int
DummyAudioBackend::connect (const std::string& src, const std::string& dst)
{
	return -1;
}

int
DummyAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	return -1;
}

int
DummyAudioBackend::connect (PortEngine::PortHandle, const std::string&)
{
	return -1;
}

int
DummyAudioBackend::disconnect (PortEngine::PortHandle, const std::string&)
{
	return -1;
}

int
DummyAudioBackend::disconnect_all (PortEngine::PortHandle)
{
	return -1;
}

bool
DummyAudioBackend::connected (PortEngine::PortHandle, bool process_callback_safe)
{
	return false;
}

bool
DummyAudioBackend::connected_to (PortEngine::PortHandle, const std::string&, bool process_callback_safe)
{
	return false;
}

bool
DummyAudioBackend::physically_connected (PortEngine::PortHandle, bool process_callback_safe)
{
	return false;
}

int
DummyAudioBackend::get_connections (PortEngine::PortHandle, std::vector<std::string>&, bool process_callback_safe)
{
	return false;
}

/* MIDI */
int
DummyAudioBackend::midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index)
{
	return -1;
}

int
DummyAudioBackend::midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size)
{
	return -1;
}

uint32_t
DummyAudioBackend::get_midi_event_count (void* port_buffer)
{
	return -1;
}

void
DummyAudioBackend::midi_clear (void* port_buffer)
{
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
DummyAudioBackend::set_latency_range (PortEngine::PortHandle, bool for_playback, LatencyRange)
{
}

LatencyRange
DummyAudioBackend::get_latency_range (PortEngine::PortHandle, bool for_playback)
{
	LatencyRange r;
	r.min = 0;
	r.max = 0;
	return r;
}

/* Discovering physical ports */

bool
DummyAudioBackend::port_is_physical (PortEngine::PortHandle) const
{
	return false;
}

void
DummyAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>&)
{
}

void
DummyAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>&)
{
}

ChanCount
DummyAudioBackend::n_physical_outputs () const
{
	ChanCount cc;
	cc.set (DataType::AUDIO, 0);
	cc.set (DataType::MIDI, 0);
	return cc;
}

ChanCount
DummyAudioBackend::n_physical_inputs () const
{
	ChanCount cc;
	cc.set (DataType::AUDIO, 0);
	cc.set (DataType::MIDI, 0);
	return cc;
}

/* Getting access to the data buffer for a port */

void*
DummyAudioBackend::get_buffer (PortEngine::PortHandle, pframes_t)
{
}

/* Engine Process */
void *
DummyAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_running = true;
	_processed_samples = 0;

	struct timeval clock1, clock2;
	::gettimeofday (&clock1, NULL);
	while (_running) {
		if (engine.process_callback (_audio_buffersize)) {
			return 0;
		}
		_processed_samples += _audio_buffersize;
		if (!_freewheeling) {
			::gettimeofday (&clock2, NULL);
			const int elapsed_time = (clock2.tv_sec - clock1.tv_sec) * 1000000 + (clock2.tv_usec - clock1.tv_usec);
			const int nomial_time = 1000000 * _audio_buffersize / _samplerate;
			_dsp_load = elapsed_time / (float) nomial_time;
			if (elapsed_time < nomial_time) {
				::usleep (nomial_time - elapsed_time);
			} else {
				::usleep (100); // don't hog cpu
			}
		} else {
			_dsp_load = 1.0;
			::usleep (100); // don't hog cpu
		}
		::gettimeofday (&clock1, NULL);
	}
	_running = false;
	return 0;
}

/******************************************************************************/

static boost::shared_ptr<DummyAudioBackend> _instance;

static boost::shared_ptr<AudioBackend>
backend_factory (AudioEngine& e)
{
	if (!_instance) {
		_instance.reset (new DummyAudioBackend (e));
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

static ARDOUR::AudioBackendInfo _descriptor = {
	"Dummy",
	instantiate,
	deinstantiate,
	backend_factory,
	already_configured,
};

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor ()
{
	return &_descriptor;
}
