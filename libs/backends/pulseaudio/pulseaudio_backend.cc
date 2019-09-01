/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "pulseaudio_backend.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

static std::string s_instance_name;

const size_t PulseAudioBackend::_max_buffer_size = 8192;

#define N_CHANNELS (2)

PulseAudioBackend::PulseAudioBackend (AudioEngine& e, AudioBackendInfo& info)
	: AudioBackend (e, info)
	, p_stream (0)
	, p_context (0)
	, p_mainloop (0)
	, _run (false)
	, _active (false)
	, _freewheel (false)
	, _freewheeling (false)
	, _last_process_start (0)
	, _samplerate (48000)
	, _samples_per_period (1024)
	, _systemic_audio_output_latency (0)
	, _dsp_load (0)
	, _processed_samples (0)
	, _port_change_flag (false)
{
	_instance_name = s_instance_name;
	pthread_mutex_init (&_port_callback_mutex, 0);
}

PulseAudioBackend::~PulseAudioBackend ()
{
	pthread_mutex_destroy (&_port_callback_mutex);
}

/* Pulseaudio */
void
PulseAudioBackend::close_pulse (bool unlock)
{
	if (p_mainloop) {
		if (unlock) {
			pa_threaded_mainloop_unlock (p_mainloop);
		}
		pa_threaded_mainloop_stop (p_mainloop);
	}

	if (p_stream) {
		pa_stream_disconnect (p_stream);
		pa_stream_unref (p_stream);
		p_stream = NULL;
	}

	if (p_context) {
		pa_context_disconnect (p_context);
		pa_context_unref (p_context);
		p_context = NULL;
	}

	if (p_mainloop) {
		pa_threaded_mainloop_free (p_mainloop);
		p_mainloop = NULL;
	}
}

int
PulseAudioBackend::sync_pulse (pa_operation* op)
{
	/* wait for async operation to complete */
	if (!op) {
		pa_threaded_mainloop_unlock (p_mainloop);
		return 0;
	}

	pa_operation_state_t state = pa_operation_get_state (op);

	while (PA_OPERATION_RUNNING == state) {
		pa_threaded_mainloop_wait (p_mainloop);
		state = pa_operation_get_state (op);
	}

	pa_operation_unref (op);

	pa_threaded_mainloop_unlock (p_mainloop);
	return PA_OPERATION_DONE == state;
}

bool
PulseAudioBackend::cork_pulse (bool pause)
{
	pa_threaded_mainloop_lock (p_mainloop);
	_operation_succeeded = false;
	return sync_pulse (pa_stream_cork (p_stream, pause ? 1 : 0, stream_operation_cb, this)) && _operation_succeeded;
}

void
PulseAudioBackend::context_state_cb (pa_context* c, void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	switch (pa_context_get_state (c)) {
		case PA_CONTEXT_READY:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			pa_threaded_mainloop_signal (d->p_mainloop, 0);
			break;
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
	}
}

void
PulseAudioBackend::stream_state_cb (pa_stream* s, void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	switch (pa_stream_get_state (s)) {
		case PA_STREAM_READY:
		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			pa_threaded_mainloop_signal (d->p_mainloop, 0);
			break;
		case PA_STREAM_UNCONNECTED:
		case PA_STREAM_CREATING:
			break;
	}
}

void
PulseAudioBackend::stream_operation_cb (pa_stream*, int ok, void* arg)
{
	PulseAudioBackend* d    = static_cast<PulseAudioBackend*> (arg);
	d->_operation_succeeded = ok;
	pa_threaded_mainloop_signal (d->p_mainloop, 0);
}

void
PulseAudioBackend::stream_request_cb (pa_stream*, size_t length, void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	pa_threaded_mainloop_signal (d->p_mainloop, 0);
	// XXX perhaps do processing here instead of waking up main callback thread.
	// compare to coreaudio backend
}

void
PulseAudioBackend::stream_latency_update_cb (pa_stream* s, void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	pa_usec_t          latency;
	int                negative;
	// XXX this needs PA_STREAM_AUTO_TIMING_UPDATE
	if (0 == pa_stream_get_latency (s, &latency, &negative)) {
		if (negative) {
			d->_systemic_audio_output_latency = 0;
		} else {
			d->_systemic_audio_output_latency = floorf (latency * d->_samplerate / 1000000.f);
		}
		// XXX garbage value
		printf ("Pulse latency update %d\n", d->_systemic_audio_output_latency);
		d->update_latencies ();
	}
	pa_threaded_mainloop_signal (d->p_mainloop, 0);
}

void
PulseAudioBackend::stream_xrun_cb (pa_stream*, void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	d->engine.Xrun ();
}

int
PulseAudioBackend::init_pulse ()
{
	pa_sample_spec ss;
	pa_buffer_attr ba;

	ss.channels = N_CHANNELS;
	ss.rate     = _samplerate;
	ss.format   = PA_SAMPLE_FLOAT32LE;

	/* https://freedesktop.org/software/pulseaudio/doxygen/structpa__buffer__attr.html */
	ba.minreq    = _samples_per_period * N_CHANNELS * sizeof (float);
	ba.maxlength = 2 * ba.minreq;
	ba.prebuf    = (uint32_t)-1;
	ba.tlength   = (uint32_t)-1;
	ba.fragsize  = 0; // capture only

	if (!pa_sample_spec_valid (&ss)) {
		return AudioDeviceInvalidError;
	}

	if (!(p_mainloop = pa_threaded_mainloop_new ())) {
		PBD::error << _("PulseAudioBackend: Failed to allocate main loop") << endmsg;
		close_pulse ();
		return BackendInitializationError;
	}

	/* see https://freedesktop.org/software/pulseaudio/doxygen/proplist_8h.html */
	pa_proplist* proplist = pa_proplist_new ();
	pa_proplist_sets (proplist, PA_PROP_MEDIA_SOFTWARE, PROGRAM_NAME);
	pa_proplist_sets (proplist, PA_PROP_MEDIA_ROLE, "production");
#if 0 // TODO
	/* in tools/linux_packaging/stage2.run.in uses xdg
	 * ICON_NAME="${PGM_VENDOR}-${PGM_NAME}_${PGM_VERSION}"
	 * e.g. "Harrison-Mixbus32C_3.7.24" "Ardour-Ardour_5.12.0"
	 *
	 * gtk2_ardour/wscript $ARDOUR_ICON is used in .desktop.in
	 * 'ardour<major>'
	 */
	pa_proplist_sets (proplist, PA_PROP_APPLICATION_ICON_NAME, "Ardour-Ardour_5.12.0");
#endif

	if (!(p_context = pa_context_new_with_proplist (pa_threaded_mainloop_get_api (p_mainloop), PROGRAM_NAME, proplist))) {
		PBD::error << _("PulseAudioBackend: Failed to allocate context") << endmsg;
		close_pulse ();
		pa_proplist_free (proplist);
		return BackendInitializationError;
	}

	pa_proplist_free (proplist);

	pa_context_set_state_callback (p_context, PulseAudioBackend::context_state_cb, this);

	if (pa_context_connect (p_context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		PBD::error << _("PulseAudioBackend: Failed to allocate context") << endmsg;
		close_pulse ();
		return AudioDeviceOpenError;
	}

	pa_threaded_mainloop_lock (p_mainloop);

	if (pa_threaded_mainloop_start (p_mainloop) < 0) {
		PBD::error << _("PulseAudioBackend: Failed to start main loop") << endmsg;
		close_pulse (true);
		return AudioDeviceOpenError;
	}

	/* Wait until the context is ready, context_state_cb will trigger this */
	pa_threaded_mainloop_wait (p_mainloop);
	if (pa_context_get_state (p_context) != PA_CONTEXT_READY) {
		PBD::error << _("PulseAudioBackend: Failed to create context") << endmsg;
		close_pulse (true);
		return AudioDeviceOpenError;
	}

	if (!(p_stream = pa_stream_new (p_context, "master", &ss, NULL))) {
		PBD::error << _("PulseAudioBackend: Failed to create new stream") << endmsg;
		close_pulse (true);
		return AudioDeviceOpenError;
	}

	pa_stream_set_state_callback (p_stream, PulseAudioBackend::stream_state_cb, this);
	pa_stream_set_write_callback (p_stream, PulseAudioBackend::stream_request_cb, this);
	pa_stream_set_latency_update_callback (p_stream, stream_latency_update_cb, this);
	pa_stream_set_underflow_callback (p_stream, PulseAudioBackend::stream_xrun_cb, this);
	pa_stream_set_overflow_callback (p_stream, PulseAudioBackend::stream_xrun_cb, this);

	/* https://freedesktop.org/software/pulseaudio/doxygen/def_8h.html#a6966d809483170bc6d2e6c16188850fc */
	pa_stream_flags_t sf = (pa_stream_flags_t) (
			  (int)PA_STREAM_START_CORKED
			| (int)PA_STREAM_FAIL_ON_SUSPEND
			/*
			| (int)PA_STREAM_ADJUST_LATENCY
			| (int)PA_STREAM_AUTO_TIMING_UPDATE
			| (int)PA_STREAM_INTERPOLATE_TIMING
			*/
			);

	if (pa_stream_connect_playback (p_stream, NULL, &ba, sf, NULL, NULL) < 0) {
		PBD::error << _("PulseAudioBackend: Failed to connect playback stream") << endmsg;
		close_pulse (true);
		return AudioDeviceOpenError;
	}

	/* Wait until the stream is ready */
	pa_threaded_mainloop_wait (p_mainloop);

	if (pa_stream_get_state (p_stream) != PA_STREAM_READY) {
		PBD::error << _("PulseAudioBackend: Failed to start stream") << endmsg;
		close_pulse (true);
		return AudioDeviceOpenError;
	}

	pa_threaded_mainloop_unlock (p_mainloop);
	return 0;
}

/* AUDIOBACKEND API */

std::string
PulseAudioBackend::name () const
{
	return X_("Pulseaudio");
}

bool
PulseAudioBackend::is_realtime () const
{
	return true;
}

std::vector<AudioBackend::DeviceStatus>
PulseAudioBackend::enumerate_devices () const
{
	std::vector<AudioBackend::DeviceStatus> devices;
	devices.push_back (DeviceStatus (_("Default Playback"), true));
	return devices;
}

std::vector<float>
PulseAudioBackend::available_sample_rates (const std::string&) const
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
PulseAudioBackend::available_buffer_sizes (const std::string&) const
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
PulseAudioBackend::available_input_channel_count (const std::string&) const
{
	return 0;
}

uint32_t
PulseAudioBackend::available_output_channel_count (const std::string&) const
{
	return N_CHANNELS;
}

bool
PulseAudioBackend::can_change_sample_rate_when_running () const
{
	return false;
}

bool
PulseAudioBackend::can_change_buffer_size_when_running () const
{
	return false;
}

int
PulseAudioBackend::set_device_name (const std::string& d)
{
	return 0;
}

int
PulseAudioBackend::set_sample_rate (float sr)
{
	if (sr <= 0) {
		return -1;
	}
	_samplerate = sr;
	engine.sample_rate_change (sr);
	return 0;
}

int
PulseAudioBackend::set_buffer_size (uint32_t bs)
{
	if (bs <= 0 || bs > _max_buffer_size) {
		return -1;
	}

	_samples_per_period = bs;

	engine.buffer_size_change (bs);
	return 0;
}

int
PulseAudioBackend::set_interleaved (bool yn)
{
	if (!yn) {
		return 0;
	}
	return -1;
}

int
PulseAudioBackend::set_input_channels (uint32_t cc)
{
	return 0;
}

int
PulseAudioBackend::set_output_channels (uint32_t cc)
{
	return 0;
}

int
PulseAudioBackend::set_systemic_input_latency (uint32_t sl)
{
	return 0;
}

int
PulseAudioBackend::set_systemic_output_latency (uint32_t sl)
{
	return 0;
}

/* Retrieving parameters */
std::string
PulseAudioBackend::device_name () const
{
	return _("Default Playback");
}

float
PulseAudioBackend::sample_rate () const
{
	return _samplerate;
}

uint32_t
PulseAudioBackend::buffer_size () const
{
	return _samples_per_period;
}

bool
PulseAudioBackend::interleaved () const
{
	return false;
}

uint32_t
PulseAudioBackend::input_channels () const
{
	return 0;
}

uint32_t
PulseAudioBackend::output_channels () const
{
	return N_CHANNELS;
}

uint32_t
PulseAudioBackend::systemic_input_latency () const
{
	return 0;
}

uint32_t
PulseAudioBackend::systemic_output_latency () const
{
	return _systemic_audio_output_latency;
}

/* MIDI */
std::vector<std::string>
PulseAudioBackend::enumerate_midi_options () const
{
	std::vector<std::string> midi_options;
	midi_options.push_back (get_standard_device_name (DeviceNone));
	return midi_options;
}

std::vector<AudioBackend::DeviceStatus>
PulseAudioBackend::enumerate_midi_devices () const
{
	return std::vector<AudioBackend::DeviceStatus> ();
}

int
PulseAudioBackend::set_midi_option (const std::string& opt)
{
	return 0;
}

std::string
PulseAudioBackend::midi_option () const
{
	return get_standard_device_name (DeviceNone);
}

/* External control app */
std::string
PulseAudioBackend::control_app_name () const
{
	std::string ignored;
	if (PBD::find_file (PBD::Searchpath (Glib::getenv("PATH")), X_("pavucontrol"), ignored)) {
		return "pavucontrol";
	}
	return "";
}

void
PulseAudioBackend::launch_control_app ()
{
#ifdef NO_VFORK
	(void) system ("pavucontrol");
#else
	if (::vfork () == 0) {
		::execlp ("pavucontrol", "pavucontrol", (char*)NULL);
		exit (EXIT_SUCCESS);
	}
#endif
}

/* State Control */

static void*
pthread_process (void* arg)
{
	PulseAudioBackend* d = static_cast<PulseAudioBackend*> (arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
PulseAudioBackend::_start (bool /*for_latency_measurement*/)
{
	if (!_active && _run) {
		PBD::error << _("PulseAudioBackend: already active.") << endmsg;
		/* recover from 'halted', reap threads */
		stop ();
	}

	if (_active || _run) {
		PBD::info << _("PulseAudioBackend: already active.") << endmsg;
		return BackendReinitializationError;
	}

	if (_ports.size () || _portmap.size ()) {
		PBD::warning << _("PulseAudioBackend: recovering from unclean shutdown, port registry is not empty.") << endmsg;
		_system_outputs.clear ();
		_ports.clear ();
		_portmap.clear ();
	}

	/* reset internal state */
	_dsp_load                      = 0;
	_freewheeling                  = false;
	_freewheel                     = false;
	_last_process_start            = 0;
	_systemic_audio_output_latency = 0;

	/* connect to pulse-server and prepare stream */
	int err = init_pulse ();
	if (err) {
		return err;
	}

	if (register_system_ports ()) {
		PBD::error << _("PulseAudioBackend: failed to register system ports.") << endmsg;
		close_pulse ();
		return PortRegistrationError;
	}

	engine.sample_rate_change (_samplerate);
	engine.buffer_size_change (_samples_per_period);

	if (engine.reestablish_ports ()) {
		PBD::error << _("PulseAudioBackend: Could not re-establish ports.") << endmsg;
		close_pulse ();
		return PortReconnectError;
	}

	engine.reconnect_ports ();

	_run = true;
	_port_change_flag = false;

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, -20, 100000,
	                                 &_main_thread, pthread_process, this)) {
		if (pthread_create (&_main_thread, NULL, pthread_process, this)) {
			PBD::error << _("PulseAudioBackend: failed to create process thread.") << endmsg;
			stop ();
			_run = false;
			return ProcessThreadStartError;
		} else {
			PBD::warning << _("PulseAudioBackend: cannot acquire realtime permissions.") << endmsg;
		}
	}

	int timeout = 5000;
	while (!_active && --timeout > 0) {
		Glib::usleep (1000);
	}

	if (timeout == 0 || !_active) {
		PBD::error << _("PulseAudioBackend: failed to start process thread.") << endmsg;
		_run = false;
		close_pulse ();
		return ProcessThreadStartError;
	}

	return NoError;
}

int
PulseAudioBackend::stop ()
{
	void* status;
	if (!_run) {
		return 0;
	}

	_run = false;

	pa_threaded_mainloop_lock (p_mainloop);
	sync_pulse (pa_stream_flush (p_stream, stream_operation_cb, this));

	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("PulseAudioBackend: failed to terminate.") << endmsg;
		return -1;
	}
	unregister_ports ();
	close_pulse ();
	return (_active == false) ? 0 : -1;
}

int
PulseAudioBackend::freewheel (bool onoff)
{
	_freewheeling = onoff;
	return 0;
}

float
PulseAudioBackend::dsp_load () const
{
	return 100.f * _dsp_load;
}

size_t
PulseAudioBackend::raw_buffer_size (DataType t)
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
PulseAudioBackend::sample_time ()
{
	return _processed_samples;
}

samplepos_t
PulseAudioBackend::sample_time_at_cycle_start ()
{
	return _processed_samples;
}

pframes_t
PulseAudioBackend::samples_since_cycle_start ()
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
PulseAudioBackend::pulse_process_thread (void* arg)
{
	ThreadData*             td = reinterpret_cast<ThreadData*> (arg);
	boost::function<void()> f  = td->f;
	delete td;
	f ();
	return 0;
}

int
PulseAudioBackend::create_process_thread (boost::function<void()> func)
{
	pthread_t      thread_id;
	pthread_attr_t attr;
	size_t         stacksize = 100000;

	ThreadData* td = new ThreadData (this, func, stacksize);

	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, -22, stacksize,
	                                 &thread_id, pulse_process_thread, td)) {
		pthread_attr_init (&attr);
		pthread_attr_setstacksize (&attr, stacksize);
		if (pthread_create (&thread_id, &attr, pulse_process_thread, td)) {
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
PulseAudioBackend::join_process_threads ()
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
PulseAudioBackend::in_process_thread ()
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
PulseAudioBackend::process_thread_count ()
{
	return _threads.size ();
}

void
PulseAudioBackend::update_latencies ()
{
	/* trigger latency callback in RT thread (locked graph) */
	port_connect_add_remove_callback ();
}

/* PORTENGINE API */

void*
PulseAudioBackend::private_handle () const
{
	return NULL;
}

const std::string&
PulseAudioBackend::my_name () const
{
	return _instance_name;
}

uint32_t
PulseAudioBackend::port_name_size () const
{
	return 256;
}

int
PulseAudioBackend::set_port_name (PortEngine::PortHandle port, const std::string& name)
{
	std::string newname (_instance_name + ":" + name);

	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::set_port_name: Invalid Port(s)") << endmsg;
		return -1;
	}

	if (find_port (newname)) {
		PBD::error << _("PulseBackend::set_port_name: Port with given name already exists") << endmsg;
		return -1;
	}

	PulsePort* p = static_cast<PulsePort*> (port);
	_portmap.erase (p->name ());
	_portmap.insert (make_pair (newname, p));
	return p->set_name (newname);
}

std::string
PulseAudioBackend::get_port_name (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::get_port_name: Invalid Port(s)") << endmsg;
		return std::string ();
	}
	return static_cast<PulsePort*> (port)->name ();
}

PortFlags
PulseAudioBackend::get_port_flags (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::get_port_flags: Invalid Port(s)") << endmsg;
		return PortFlags (0);
	}
	return static_cast<PulsePort*> (port)->flags ();
}

int
PulseAudioBackend::get_port_property (PortHandle port, const std::string& key, std::string& value, std::string& type) const
{
	if (!valid_port (port)) {
		PBD::warning << _("PulseBackend::get_port_property: Invalid Port(s)") << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name") {
		type  = "";
		value = static_cast<PulsePort*> (port)->pretty_name ();
		if (!value.empty ()) {
			return 0;
		}
	}
	return -1;
}

int
PulseAudioBackend::set_port_property (PortHandle port, const std::string& key, const std::string& value, const std::string& type)
{
	if (!valid_port (port)) {
		PBD::warning << _("PulseBackend::set_port_property: Invalid Port(s)") << endmsg;
		return -1;
	}
	if (key == "http://jackaudio.org/metadata/pretty-name" && type.empty ()) {
		static_cast<PulsePort*> (port)->set_pretty_name (value);
		return 0;
	}
	return -1;
}

PortEngine::PortHandle
PulseAudioBackend::get_port_by_name (const std::string& name) const
{
	PortHandle port = (PortHandle)find_port (name);
	return port;
}

int
PulseAudioBackend::get_ports (
    const std::string& port_name_pattern,
    DataType type, PortFlags flags,
    std::vector<std::string>& port_names) const
{
	int     rv = 0;
	regex_t port_regex;
	bool    use_regexp = false;
	if (port_name_pattern.size () > 0) {
		if (!regcomp (&port_regex, port_name_pattern.c_str (), REG_EXTENDED | REG_NOSUB)) {
			use_regexp = true;
		}
	}

	for (PortIndex::const_iterator i = _ports.begin (); i != _ports.end (); ++i) {
		PulsePort* port = *i;
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
PulseAudioBackend::port_data_type (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		return DataType::NIL;
	}
	return static_cast<PulsePort*> (port)->type ();
}

PortEngine::PortHandle
PulseAudioBackend::register_port (
    const std::string& name,
    ARDOUR::DataType   type,
    ARDOUR::PortFlags  flags)
{
	if (name.size () == 0) {
		return 0;
	}
	if (flags & IsPhysical) {
		return 0;
	}
	return add_port (_instance_name + ":" + name, type, flags);
}

PortEngine::PortHandle
PulseAudioBackend::add_port (
    const std::string& name,
    ARDOUR::DataType   type,
    ARDOUR::PortFlags  flags)
{
	assert (name.size ());
	if (find_port (name)) {
		PBD::error << _("PulseBackend::register_port: Port already exists:")
		           << " (" << name << ")" << endmsg;
		return 0;
	}
	PulsePort* port = NULL;
	switch (type) {
		case DataType::AUDIO:
			port = new PulseAudioPort (*this, name, flags);
			break;
		case DataType::MIDI:
			port = new PulseMidiPort (*this, name, flags);
			break;
		default:
			PBD::error << _("PulseBackend::register_port: Invalid Data Type.") << endmsg;
			return 0;
	}

	_ports.insert (port);
	_portmap.insert (make_pair (name, port));

	return port;
}

void
PulseAudioBackend::unregister_port (PortEngine::PortHandle port_handle)
{
	if (!_run) {
		return;
	}
	PulsePort*          port = static_cast<PulsePort*> (port_handle);
	PortIndex::iterator i    = std::find (_ports.begin (), _ports.end (), static_cast<PulsePort*> (port_handle));
	if (i == _ports.end ()) {
		PBD::error << _("PulseBackend::unregister_port: Failed to find port") << endmsg;
		return;
	}
	disconnect_all (port_handle);
	_portmap.erase (port->name ());
	_ports.erase (i);
	delete port;
}

int
PulseAudioBackend::register_system_ports ()
{
	LatencyRange lr;
	/* audio ports */
	lr.min = lr.max = _systemic_audio_output_latency;
	for (int i = 1; i <= N_CHANNELS; ++i) {
		char tmp[64];
		snprintf (tmp, sizeof (tmp), "system:playback_%d", i);
		PortHandle p = add_port (std::string (tmp), DataType::AUDIO, static_cast<PortFlags> (IsInput | IsPhysical | IsTerminal));
		if (!p)
			return -1;
		set_latency_range (p, true, lr);
		PulsePort* ap = static_cast<PulsePort*> (p);
		//ap->set_pretty_name ("")
		_system_outputs.push_back (ap);
	}
	return 0;
}

void
PulseAudioBackend::unregister_ports (bool system_only)
{
	_system_outputs.clear ();

	for (PortIndex::iterator i = _ports.begin (); i != _ports.end ();) {
		PortIndex::iterator cur  = i++;
		PulsePort*          port = *cur;
		if (!system_only || (port->is_physical () && port->is_terminal ())) {
			port->disconnect_all ();
			_portmap.erase (port->name ());
			delete port;
			_ports.erase (cur);
		}
	}
}

void
PulseAudioBackend::update_system_port_latecies ()
{
	for (std::vector<PulsePort*>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it) {
		(*it)->update_connected_latency (false);
	}
}

int
PulseAudioBackend::connect (const std::string& src, const std::string& dst)
{
	PulsePort* src_port = find_port (src);
	PulsePort* dst_port = find_port (dst);

	if (!src_port) {
		PBD::error << _("PulseBackend::connect: Invalid Source port:")
		           << " (" << src << ")" << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("PulseBackend::connect: Invalid Destination port:")
		           << " (" << dst << ")" << endmsg;
		return -1;
	}
	return src_port->connect (dst_port);
}

int
PulseAudioBackend::disconnect (const std::string& src, const std::string& dst)
{
	PulsePort* src_port = find_port (src);
	PulsePort* dst_port = find_port (dst);

	if (!src_port || !dst_port) {
		PBD::error << _("PulseBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return src_port->disconnect (dst_port);
}

int
PulseAudioBackend::connect (PortEngine::PortHandle src, const std::string& dst)
{
	PulsePort* dst_port = find_port (dst);
	if (!valid_port (src)) {
		PBD::error << _("PulseBackend::connect: Invalid Source Port Handle") << endmsg;
		return -1;
	}
	if (!dst_port) {
		PBD::error << _("PulseBackend::connect: Invalid Destination Port")
		           << " (" << dst << ")" << endmsg;
		return -1;
	}
	return static_cast<PulsePort*> (src)->connect (dst_port);
}

int
PulseAudioBackend::disconnect (PortEngine::PortHandle src, const std::string& dst)
{
	PulsePort* dst_port = find_port (dst);
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("PulseBackend::disconnect: Invalid Port(s)") << endmsg;
		return -1;
	}
	return static_cast<PulsePort*> (src)->disconnect (dst_port);
}

int
PulseAudioBackend::disconnect_all (PortEngine::PortHandle port)
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::disconnect_all: Invalid Port") << endmsg;
		return -1;
	}
	static_cast<PulsePort*> (port)->disconnect_all ();
	return 0;
}

bool
PulseAudioBackend::connected (PortEngine::PortHandle port, bool /* process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::disconnect_all: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<PulsePort*> (port)->is_connected ();
}

bool
PulseAudioBackend::connected_to (PortEngine::PortHandle src, const std::string& dst, bool /*process_callback_safe*/)
{
	PulsePort* dst_port = find_port (dst);
#ifndef NDEBUG
	if (!valid_port (src) || !dst_port) {
		PBD::error << _("PulseBackend::connected_to: Invalid Port") << endmsg;
		return false;
	}
#endif
	return static_cast<PulsePort*> (src)->is_connected (dst_port);
}

bool
PulseAudioBackend::physically_connected (PortEngine::PortHandle port, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::physically_connected: Invalid Port") << endmsg;
		return false;
	}
	return static_cast<PulsePort*> (port)->is_physically_connected ();
}

int
PulseAudioBackend::get_connections (PortEngine::PortHandle port, std::vector<std::string>& names, bool /*process_callback_safe*/)
{
	if (!valid_port (port)) {
		PBD::error << _("PulseBackend::get_connections: Invalid Port") << endmsg;
		return -1;
	}

	assert (0 == names.size ());

	const std::set<PulsePort*>& connected_ports = static_cast<PulsePort*> (port)->get_connections ();

	for (std::set<PulsePort*>::const_iterator i = connected_ports.begin (); i != connected_ports.end (); ++i) {
		names.push_back ((*i)->name ());
	}

	return (int)names.size ();
}

/* MIDI */
int
PulseAudioBackend::midi_event_get (
    pframes_t& timestamp,
    size_t& size, uint8_t const** buf, void* port_buffer,
    uint32_t event_index)
{
	assert (buf && port_buffer);
	PulseMidiBuffer& source = *static_cast<PulseMidiBuffer*> (port_buffer);
	if (event_index >= source.size ()) {
		return -1;
	}
	PulseMidiEvent* const event = source[event_index].get ();

	timestamp = event->timestamp ();
	size      = event->size ();
	*buf      = event->data ();
	return 0;
}

int
PulseAudioBackend::midi_event_put (
    void*          port_buffer,
    pframes_t      timestamp,
    const uint8_t* buffer, size_t size)
{
	assert (buffer && port_buffer);
	PulseMidiBuffer& dst = *static_cast<PulseMidiBuffer*> (port_buffer);
	dst.push_back (boost::shared_ptr<PulseMidiEvent> (new PulseMidiEvent (timestamp, buffer, size)));
	return 0;
}

uint32_t
PulseAudioBackend::get_midi_event_count (void* port_buffer)
{
	assert (port_buffer);
	return static_cast<PulseMidiBuffer*> (port_buffer)->size ();
}

void
PulseAudioBackend::midi_clear (void* port_buffer)
{
	assert (port_buffer);
	PulseMidiBuffer* buf = static_cast<PulseMidiBuffer*> (port_buffer);
	assert (buf);
	buf->clear ();
}

/* Monitoring */

bool
PulseAudioBackend::can_monitor_input () const
{
	return false;
}

int
PulseAudioBackend::request_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

int
PulseAudioBackend::ensure_input_monitoring (PortEngine::PortHandle, bool)
{
	return -1;
}

bool
    PulseAudioBackend::monitoring_input (PortEngine::PortHandle)
{
	return false;
}

/* Latency management */

void
PulseAudioBackend::set_latency_range (PortEngine::PortHandle port, bool for_playback, LatencyRange latency_range)
{
	if (!valid_port (port)) {
		PBD::error << _("PulsePort::set_latency_range (): invalid port.") << endmsg;
	}
	static_cast<PulsePort*> (port)->set_latency_range (latency_range, for_playback);
}

LatencyRange
PulseAudioBackend::get_latency_range (PortEngine::PortHandle port, bool for_playback)
{
	LatencyRange r;
	if (!valid_port (port)) {
		PBD::error << _("PulsePort::get_latency_range (): invalid port.") << endmsg;
		r.min = 0;
		r.max = 0;
		return r;
	}
	PulsePort* p = static_cast<PulsePort*> (port);
	assert (p);

	r = p->latency_range (for_playback);
	if (p->is_physical () && p->is_terminal ()) {
		if (p->is_input () && for_playback) {
			r.min += _samples_per_period + _systemic_audio_output_latency;
			r.max += _samples_per_period + _systemic_audio_output_latency;
		}
		if (p->is_output () && !for_playback) {
			r.min += _samples_per_period;
			r.max += _samples_per_period;
		}
	}
	return r;
}

/* Discovering physical ports */

bool
PulseAudioBackend::port_is_physical (PortEngine::PortHandle port) const
{
	if (!valid_port (port)) {
		PBD::error << _("PulsePort::port_is_physical (): invalid port.") << endmsg;
		return false;
	}
	return static_cast<PulsePort*> (port)->is_physical ();
}

void
PulseAudioBackend::get_physical_outputs (DataType type, std::vector<std::string>& port_names)
{
	for (PortIndex::iterator i = _ports.begin (); i != _ports.end (); ++i) {
		PulsePort* port = *i;
		if ((port->type () == type) && port->is_input () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
}

void
PulseAudioBackend::get_physical_inputs (DataType type, std::vector<std::string>& port_names)
{
	for (PortIndex::iterator i = _ports.begin (); i != _ports.end (); ++i) {
		PulsePort* port = *i;
		if ((port->type () == type) && port->is_output () && port->is_physical ()) {
			port_names.push_back (port->name ());
		}
	}
	assert (port_names.size () == 0);
}

ChanCount
PulseAudioBackend::n_physical_outputs () const
{
	int n_midi  = 0;
	int n_audio = 0;
	for (PortIndex::const_iterator i = _ports.begin (); i != _ports.end (); ++i) {
		PulsePort* port = *i;
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
PulseAudioBackend::n_physical_inputs () const
{
	int n_midi  = 0;
	int n_audio = 0;
	for (PortIndex::const_iterator i = _ports.begin (); i != _ports.end (); ++i) {
		PulsePort* port = *i;
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
PulseAudioBackend::get_buffer (PortEngine::PortHandle port, pframes_t nframes)
{
	assert (port);
	assert (valid_port (port));
	return static_cast<PulsePort*> (port)->get_buffer (nframes);
}

/* Engine Process */
void*
PulseAudioBackend::main_process_thread ()
{
	AudioEngine::thread_init_callback (this);
	_active            = true;
	_processed_samples = 0;

	manager.registration_callback ();
	manager.graph_order_callback ();

	manager.registration_callback ();
	manager.graph_order_callback ();

	/* flush stream */
	pa_threaded_mainloop_lock (p_mainloop);
	sync_pulse (pa_stream_flush (p_stream, stream_operation_cb, this));

	/* begin streaming */
	if (!cork_pulse (false)) {
		_active = false;
		if (_run) {
			engine.halted_callback ("PulseAudio: cannot uncork stream");
		}
	}

	stream_latency_update_cb (p_stream, this);

	while (_run) {
		if (_freewheeling != _freewheel) {
			_freewheel = _freewheeling;
			engine.freewheel_callback (_freewheel);

			/* drain stream freewheeling */
			pa_threaded_mainloop_lock (p_mainloop);
			_operation_succeeded = false;
			if (!sync_pulse (pa_stream_drain (p_stream, stream_operation_cb, this)) || !_operation_succeeded) {
				break;
			}

			/* suspend output while freewheeling, re-anable after */
			if (!cork_pulse (_freewheel)) {
				break;
			}
			if (!_freewheel) {
				pa_threaded_mainloop_lock (p_mainloop);
				_operation_succeeded = false;
				if (!sync_pulse (pa_stream_flush (p_stream, stream_operation_cb, this)) || !_operation_succeeded) {
					break;
				}
			}
		}

		if (!_freewheel) {
			pa_threaded_mainloop_lock (p_mainloop);

			size_t bytes_to_write = sizeof (float) * _samples_per_period * N_CHANNELS;
			if (pa_stream_writable_size (p_stream) < bytes_to_write) {
				/* wait until stream_request_cb triggers */
				pa_threaded_mainloop_wait (p_mainloop);
			}

			if (pa_stream_get_state (p_stream) != PA_STREAM_READY) {
				pa_threaded_mainloop_unlock (p_mainloop);
				break;
			}

			int64_t clock1 = g_get_monotonic_time ();
			/* call engine process callback */
			_last_process_start = g_get_monotonic_time ();
			if (engine.process_callback (_samples_per_period)) {
				pa_threaded_mainloop_unlock (p_mainloop);
				_active = false;
				return 0;
			}

			/* write back audio */
			uint32_t i = 0;
			float    buf[_max_buffer_size * N_CHANNELS];
			assert (_system_outputs.size () == N_CHANNELS);

			/* interleave */
			for (std::vector<PulsePort *>::const_iterator it = _system_outputs.begin (); it != _system_outputs.end (); ++it, ++i) {
				const float* src = (const float*)(*it)->get_buffer (_samples_per_period);
				for (size_t n = 0; n < _samples_per_period; ++n) {
					buf[N_CHANNELS * n + i] = src[n];
				}
			}

			if (pa_stream_write (p_stream, buf, bytes_to_write, NULL, 0, PA_SEEK_RELATIVE) < 0) {
				pa_threaded_mainloop_unlock (p_mainloop);
				break;
			}
			pa_threaded_mainloop_unlock (p_mainloop);

			_processed_samples += _samples_per_period;

			_dsp_load_calc.set_max_time (_samplerate, _samples_per_period);
			_dsp_load_calc.set_start_timestamp_us (clock1);
			_dsp_load_calc.set_stop_timestamp_us (g_get_monotonic_time ());
			_dsp_load = _dsp_load_calc.get_dsp_load ();
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
			if (_port_change_flag) {
				ports_changed     = true;
				_port_change_flag = false;
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
			update_system_port_latecies ();
			engine.latency_callback (false);
			engine.latency_callback (true);
		}
	}

	_active = false;
	if (_run) {
		engine.halted_callback ("PulseAudio I/O error.");
	}
	return 0;
}

/******************************************************************************/

static boost::shared_ptr<PulseAudioBackend> _instance;

static boost::shared_ptr<AudioBackend> backend_factory (AudioEngine& e);
static int instantiate (const std::string& arg1, const std::string& /* arg2 */);
static int  deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
	_("Pulseaudio"),
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
		_instance.reset (new PulseAudioBackend (e, _descriptor));
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

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo*
descriptor ()
{
	return &_descriptor;
}

/******************************************************************************/
PulsePort::PulsePort (PulseAudioBackend& b, const std::string& name, PortFlags flags)
    : _pulse_backend (b)
    , _name (name)
    , _flags (flags)
{
	_capture_latency_range.min  = 0;
	_capture_latency_range.max  = 0;
	_playback_latency_range.min = 0;
	_playback_latency_range.max = 0;
	_pulse_backend.port_connect_add_remove_callback (); // XXX -> RT
}

PulsePort::~PulsePort ()
{
	disconnect_all ();
	_pulse_backend.port_connect_add_remove_callback (); // XXX -> RT
}

int
PulsePort::connect (PulsePort* port)
{
	if (!port) {
		PBD::error << _("PulsePort::connect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (type () != port->type ()) {
		PBD::error << _("PulsePort::connect (): wrong port-type") << endmsg;
		return -1;
	}

	if (is_output () && port->is_output ()) {
		PBD::error << _("PulsePort::connect (): cannot inter-connect output ports.") << endmsg;
		return -1;
	}

	if (is_input () && port->is_input ()) {
		PBD::error << _("PulsePort::connect (): cannot inter-connect input ports.") << endmsg;
		return -1;
	}

	if (this == port) {
		PBD::error << _("PulsePort::connect (): cannot self-connect ports.") << endmsg;
		return -1;
	}

	if (is_connected (port)) {
		return -1;
	}

	_connect (port, true);
	return 0;
}

void
PulsePort::_connect (PulsePort* port, bool callback)
{
	_connections.insert (port);
	if (callback) {
		port->_connect (this, false);
		_pulse_backend.port_connect_callback (name (), port->name (), true);
	}
}

int
PulsePort::disconnect (PulsePort* port)
{
	if (!port) {
		PBD::error << _("PulsePort::disconnect (): invalid (null) port") << endmsg;
		return -1;
	}

	if (!is_connected (port)) {
		PBD::error << _("PulsePort::disconnect (): ports are not connected:")
		           << " (" << name () << ") -> (" << port->name () << ")"
		           << endmsg;
		return -1;
	}
	_disconnect (port, true);
	return 0;
}

void
PulsePort::_disconnect (PulsePort* port, bool callback)
{
	std::set<PulsePort*>::iterator it = _connections.find (port);
	assert (it != _connections.end ());
	_connections.erase (it);
	if (callback) {
		port->_disconnect (this, false);
		_pulse_backend.port_connect_callback (name (), port->name (), false);
	}
}

void
PulsePort::disconnect_all ()
{
	while (!_connections.empty ()) {
		std::set<PulsePort*>::iterator it = _connections.begin ();
		(*it)->_disconnect (this, false);
		_pulse_backend.port_connect_callback (name (), (*it)->name (), false);
		_connections.erase (it);
	}
}

bool
PulsePort::is_connected (const PulsePort* port) const
{
	return _connections.find (const_cast<PulsePort*> (port)) != _connections.end ();
}

bool
PulsePort::is_physically_connected () const
{
	for (std::set<PulsePort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			return true;
		}
	}
	return false;
}

void
PulsePort::set_latency_range (const LatencyRange& latency_range, bool for_playback)
{
	if (for_playback) {
		_playback_latency_range = latency_range;
	} else {
		_capture_latency_range = latency_range;
	}

	for (std::set<PulsePort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		if ((*it)->is_physical ()) {
			(*it)->update_connected_latency (is_input ());
		}
	}
}

void
PulsePort::update_connected_latency (bool for_playback)
{
	LatencyRange lr;
	lr.min = lr.max = 0;
	for (std::set<PulsePort*>::const_iterator it = _connections.begin (); it != _connections.end (); ++it) {
		LatencyRange l;
		l      = (*it)->latency_range (for_playback);
		lr.min = std::max (lr.min, l.min);
		lr.max = std::max (lr.max, l.max);
	}
	set_latency_range (lr, for_playback);
}

/******************************************************************************/

PulseAudioPort::PulseAudioPort (PulseAudioBackend& b, const std::string& name, PortFlags flags)
    : PulsePort (b, name, flags)
{
	memset (_buffer, 0, sizeof (_buffer));
	mlock (_buffer, sizeof (_buffer));
}

PulseAudioPort::~PulseAudioPort ()
{
}

void*
PulseAudioPort::get_buffer (pframes_t n_samples)
{
	if (is_input ()) {
		const std::set<PulsePort*>&          connections = get_connections ();
		std::set<PulsePort*>::const_iterator it          = connections.begin ();
		if (it == connections.end ()) {
			memset (_buffer, 0, n_samples * sizeof (Sample));
		} else {
			PulseAudioPort* source = static_cast<PulseAudioPort*> (*it);
			assert (source && source->is_output ());
			memcpy (_buffer, source->const_buffer (), n_samples * sizeof (Sample));
			while (++it != connections.end ()) {
				source = static_cast<PulseAudioPort*> (*it);
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

PulseMidiPort::PulseMidiPort (PulseAudioBackend& b, const std::string& name, PortFlags flags)
    : PulsePort (b, name, flags)
{
	_buffer.clear ();
	_buffer.reserve (256);
}

PulseMidiPort::~PulseMidiPort ()
{
}

struct MidiEventSorter {
	bool
	operator() (const boost::shared_ptr<PulseMidiEvent>& a, const boost::shared_ptr<PulseMidiEvent>& b)
	{
		return *a < *b;
	}
};

void* PulseMidiPort::get_buffer (pframes_t /*n_samples*/)
{
	if (is_input ()) {
		_buffer.clear ();
		const std::set<PulsePort*>& connections = get_connections ();
		for (std::set<PulsePort*>::const_iterator i = connections.begin ();
		     i != connections.end ();
		     ++i) {
			const PulseMidiBuffer* src = static_cast<PulseMidiPort*> (*i)->const_buffer ();
			for (PulseMidiBuffer::const_iterator it = src->begin (); it != src->end (); ++it) {
				_buffer.push_back (*it);
			}
		}
		std::stable_sort (_buffer.begin (), _buffer.end (), MidiEventSorter ());
	}
	return &_buffer;
}

PulseMidiEvent::PulseMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size)
    : _size (size)
    , _timestamp (timestamp)
{
	if (size > 0 && size < MaxPulseMidiEventSize) {
		memcpy (_data, data, size);
	}
}

PulseMidiEvent::PulseMidiEvent (const PulseMidiEvent& other)
    : _size (other.size ())
    , _timestamp (other.timestamp ())
{
	if (other.size () && other.const_data ()) {
		assert (other._size < MaxPulseMidiEventSize);
		memcpy (_data, other._data, other._size);
	}
};
