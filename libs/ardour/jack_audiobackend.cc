/*
    Copyright (C) 2013 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <math.h>

#include <boost/scoped_ptr.hpp>
#include <glibmm/timer.h>

#include "pbd/error.h"

#include "midi++/manager.h"

#include "ardour/audioengine.h"
#include "ardour/types.h"
#include "ardour/jack_audiobackend.h"
#include "ardour/jack_connection.h"
#include "ardour/jack_portengine.h"
#include "ardour/jack_utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;
using std::vector;

#define GET_PRIVATE_JACK_POINTER(localvar)  jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(localvar,r) jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return r; }

JACKAudioBackend::JACKAudioBackend (AudioEngine& e, boost::shared_ptr<JackConnection> jc)
	: AudioBackend (e)
	, _jack_connection (jc)
	, _running (false)
	, _freewheeling (false)
	, _target_sample_rate (48000)
	, _target_buffer_size (1024)
	, _target_sample_format (FormatFloat)
	, _target_interleaved (false)
	, _target_input_channels (-1)
	, _target_output_channels (-1)
	, _target_systemic_input_latency (0)
	, _target_systemic_output_latency (0)
{
}

JACKAudioBackend::~JACKAudioBackend()
{
}

string
JACKAudioBackend::name() const 
{
	return X_("JACK");
}

void*
JACKAudioBackend::private_handle() const
{
	return _jack_connection->jack();
}

bool
JACKAudioBackend::connected() const
{
	return (private_handle() != 0);
}

bool
JACKAudioBackend::is_realtime () const
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack,false);
	return jack_is_realtime (_priv_jack);
}

bool
JACKAudioBackend::requires_driver_selection() const
{
	return true;
}

vector<string>
JACKAudioBackend::enumerate_drivers () const
{
	vector<string> s;
	get_jack_audio_driver_names (s);
	return s;
}

int
JACKAudioBackend::set_driver (const std::string& name)
{
	_target_driver = name;
	return 0;
}

vector<string>
JACKAudioBackend::enumerate_devices () const
{
	return get_jack_device_names_for_audio_driver (_target_driver);
}

vector<float>
JACKAudioBackend::available_sample_rates (const string& /*device*/) const
{
	vector<float> f;
	
	if (connected()) {
		f.push_back (sample_rate());
		return f;
	}

	/* if JACK is not already running, just list a bunch of reasonable 
	   values and let the future sort it all out.
	*/

	f.push_back (8000.0);
	f.push_back (16000.0);
	f.push_back (24000.0);
	f.push_back (32000.0);
	f.push_back (44100.0);
	f.push_back (48000.0);
	f.push_back (88200.0);
	f.push_back (96000.0);
	f.push_back (192000.0);
	f.push_back (384000.0);
	
	return f;
}

vector<uint32_t>
JACKAudioBackend::available_buffer_sizes (const string& /*device*/) const
{
	vector<uint32_t> s;
	
	if (connected()) {
		s.push_back (buffer_size());
		return s;
	}

	s.push_back (8);
	s.push_back (16);
	s.push_back (32);
	s.push_back (64);
	s.push_back (128);
	s.push_back (256);
	s.push_back (512);
	s.push_back (1024);
	s.push_back (2048);
	s.push_back (4096);
	s.push_back (8192);

	return s;
}

uint32_t
JACKAudioBackend::available_input_channel_count (const string& /*device*/) const
{
	return 128;
}

uint32_t
JACKAudioBackend::available_output_channel_count (const string& /*device*/) const
{
	return 128;
}

/* -- parameter setting -- */

int
JACKAudioBackend::set_device_name (const string& dev)
{
	if (connected()) {
		/* need to stop and restart JACK for this to work, at present */
		return -1;
	}

	_target_device = dev;
	return 0;
}

int
JACKAudioBackend::set_sample_rate (float sr)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	
	if (!connected()) {
		_target_sample_rate = sr;
		return 0;
	}

	if (sr == jack_get_sample_rate (_priv_jack)) {
                return 0;
	}

	return -1;
}

int
JACKAudioBackend::set_buffer_size (uint32_t nframes)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);

	if (!connected()) {
		_target_buffer_size = nframes;
		return 0;
	}

	if (nframes == jack_get_buffer_size (_priv_jack)) {
                return 0;
	}

	return jack_set_buffer_size (_priv_jack, nframes);
}

int
JACKAudioBackend::set_sample_format (SampleFormat sf)
{
	/* as far as JACK clients are concerned, the hardware is always
	 * floating point format.
	 */
	if (sf == FormatFloat) {
		return 0;
	}
	return -1;
}

int
JACKAudioBackend::set_interleaved (bool yn)
{
	/* as far as JACK clients are concerned, the hardware is always
	 * non-interleaved
	 */
	if (!yn) {
		return 0;
	}
	return -1;
}

int
JACKAudioBackend::set_input_channels (uint32_t cnt)
{
	if (connected()) {
		return -1;
	}

	_target_input_channels = cnt;
	
	return 0;
}

int
JACKAudioBackend::set_output_channels (uint32_t cnt)
{
	if (connected()) {
		return -1;
	}

	_target_output_channels = cnt;

	return 0;
}

int
JACKAudioBackend::set_systemic_input_latency (uint32_t l)
{
	if (connected()) {
		return -1;
	}

	_target_systemic_input_latency = l;

	return 0;
}

int
JACKAudioBackend::set_systemic_output_latency (uint32_t l)
{
	if (connected()) {
		return -1;
	}

	_target_systemic_output_latency = l;

	return 0;
}

/* --- Parameter retrieval --- */

std::string
JACKAudioBackend::device_name () const
{
	return string();
}

float
JACKAudioBackend::sample_rate () const
{
	if (connected()) {
		return _current_sample_rate;
	}
	return _target_sample_rate;
}

uint32_t
JACKAudioBackend::buffer_size () const
{
	if (connected()) {
		return _current_buffer_size;
	}
	return _target_buffer_size;
}

SampleFormat
JACKAudioBackend::sample_format () const
{
	return FormatFloat;
}

bool
JACKAudioBackend::interleaved () const
{
	return false;
}

uint32_t
JACKAudioBackend::input_channels () const
{
	if (connected()) {
		return n_physical (JackPortIsInput).n_audio();
	} 
	return _target_input_channels;
}

uint32_t
JACKAudioBackend::output_channels () const
{
	if (connected()) {
		return n_physical (JackPortIsOutput).n_audio();
	} 
	return _target_output_channels;
}

uint32_t
JACKAudioBackend::systemic_input_latency () const
{
	return _current_systemic_output_latency;
}

uint32_t
JACKAudioBackend::systemic_output_latency () const
{
	return _current_systemic_output_latency;
}

size_t 
JACKAudioBackend::raw_buffer_size(DataType t)
{
	std::map<DataType,size_t>::const_iterator s = _raw_buffer_sizes.find(t);
	return (s != _raw_buffer_sizes.end()) ? s->second : 0;
}

void
JACKAudioBackend::setup_jack_startup_command ()
{
	/* first we map the parameters that have been set onto a
	 * JackCommandLineOptions object.
	 */

	JackCommandLineOptions options;

	options.samplerate = _target_sample_rate;
	options.period_size = _target_buffer_size;
	options.num_periods = 2;
	options.input_device = _target_device;
	options.output_device = _target_device;
	options.input_latency = _target_systemic_input_latency;
	options.output_latency = _target_systemic_output_latency;
	if (_target_sample_format == FormatInt16) {
		options.force16_bit = _target_sample_format;
	}

	/* this must always be true for any server instance we start ourselves
	 */

	options.temporary = true;

	string cmdline;

	if (!get_jack_command_line_string (options, cmdline)) {
		/* error, somehow */
	}

	std::cerr << "JACK command line will be: " << cmdline << std::endl;

	// write_jack_config_file (get_jack_server_user_config_file_path(), cmdline);
}

/* ---- BASIC STATE CONTROL API: start/stop/pause/freewheel --- */

int
JACKAudioBackend::start ()
{
	if (!connected()) {

		if (!_jack_connection->server_running()) {
			setup_jack_startup_command ();
		}

		std::cerr << "Open JACK connection\n";
		_jack_connection->open ();
	}

	engine.reestablish_ports ();
	
	if (!jack_port_type_get_buffer_size) {
		warning << _("This version of JACK is old - you should upgrade to a newer version that supports jack_port_type_get_buffer_size()") << endmsg;
	}
	
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);

	engine.sample_rate_change (jack_get_sample_rate (_priv_jack));
	
	/* testing the nullity of this function name is a proxy for
	 * whether jack_activate() will definitely call the buffer size
	 * callback. with older versions of JACK, this function symbol
	 * will be null.  this is sort of reliable, but not clean since
	 * weak symbol support is highly platform and compiler
	 * specific.
	 */
	if (!jack_port_type_get_buffer_size) {
		jack_bufsize_callback (jack_get_buffer_size (_priv_jack));
	}
	
	set_jack_callbacks ();
	
	if (jack_activate (_priv_jack) == 0) {
		_running = true;
	} else {
		// error << _("cannot activate JACK client") << endmsg;
	}
	
	engine.reconnect_ports ();

	return 0;
}

int
JACKAudioBackend::stop ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	
	_jack_connection->close ();

	_current_buffer_size = 0;
	_current_sample_rate = 0;

	_raw_buffer_sizes.clear();

	return 0;
}

int
JACKAudioBackend::pause ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);

	if (_priv_jack) {
		jack_deactivate (_priv_jack);
	}

	return 0;
}

int
JACKAudioBackend::freewheel (bool onoff)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);

	if (onoff == _freewheeling) {
		/* already doing what has been asked for */
		
		return 0;
	}

	if (jack_set_freewheel (_priv_jack, onoff) == 0) {
		_freewheeling = true;
		return 0;
	}

	return -1;
}

/* --- TRANSPORT STATE MANAGEMENT --- */

void
JACKAudioBackend::transport_stop ()
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	jack_transport_stop (_priv_jack);
}

void
JACKAudioBackend::transport_start ()
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	jack_transport_start (_priv_jack);
}

void
JACKAudioBackend::transport_locate (framepos_t where)
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	jack_transport_locate (_priv_jack, where);
}

framepos_t 
JACKAudioBackend::transport_frame () const 
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_get_current_transport_frame (_priv_jack);
}

TransportState
JACKAudioBackend::transport_state () const
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, ((TransportState) JackTransportStopped));
	jack_position_t pos;
	return (TransportState) jack_transport_query (_priv_jack, &pos);
}

int
JACKAudioBackend::set_time_master (bool yn)
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, -1);
	if (yn) {
		return jack_set_timebase_callback (_priv_jack, 0, _jack_timebase_callback, this);
	} else {
		return jack_release_timebase (_priv_jack);
	}
}

/* process-time */

bool
JACKAudioBackend::get_sync_offset (pframes_t& offset) const
{

#ifdef HAVE_JACK_VIDEO_SUPPORT

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, false);

	jack_position_t pos;

	if (_priv_jack) {
		(void) jack_transport_query (_priv_jack, &pos);

		if (pos.valid & JackVideoFrameOffset) {
			offset = pos.video_offset;
			return true;
		}
	}
#else
	/* keep gcc happy */
	offset = 0;
#endif

	return false;
}

pframes_t
JACKAudioBackend::sample_time ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_frame_time (_priv_jack);
}

pframes_t
JACKAudioBackend::sample_time_at_cycle_start ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_last_frame_time (_priv_jack);
}

pframes_t
JACKAudioBackend::samples_since_cycle_start ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_frames_since_cycle_start (_priv_jack);
}

/* JACK Callbacks */

static void
ardour_jack_error (const char* msg)
{
	error << "JACK: " << msg << endmsg;
}

void
JACKAudioBackend::set_jack_callbacks ()
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);

        jack_set_thread_init_callback (_priv_jack, AudioEngine::thread_init_callback, 0);

        jack_set_process_thread (_priv_jack, _process_thread, this);
        jack_set_sample_rate_callback (_priv_jack, _sample_rate_callback, this);
        jack_set_buffer_size_callback (_priv_jack, _bufsize_callback, this);
        jack_set_xrun_callback (_priv_jack, _xrun_callback, this);
        jack_set_sync_callback (_priv_jack, _jack_sync_callback, this);
        jack_set_freewheel_callback (_priv_jack, _freewheel_callback, this);

#ifdef HAVE_JACK_SESSION
        if( jack_set_session_callback)
                jack_set_session_callback (_priv_jack, _session_callback, this);
#endif

        if (jack_set_latency_callback) {
                jack_set_latency_callback (_priv_jack, _latency_callback, this);
        }

        jack_set_error_function (ardour_jack_error);
}

void
JACKAudioBackend::_jack_timebase_callback (jack_transport_state_t state, pframes_t nframes,
				      jack_position_t* pos, int new_position, void *arg)
{
	static_cast<JACKAudioBackend*> (arg)->jack_timebase_callback (state, nframes, pos, new_position);
}

void
JACKAudioBackend::jack_timebase_callback (jack_transport_state_t state, pframes_t /*nframes*/,
					  jack_position_t* pos, int /*new_position*/)
{
	TransportState tstate;
	framepos_t position;

	switch (state) {
	case JackTransportStopped:
		tstate = TransportStopped;
		break;
	case JackTransportRolling:
		tstate = TransportRolling;
		break;
	case JackTransportLooping:
		tstate = TransportLooping;
		break;
	case JackTransportStarting:
		tstate = TransportStarting;
		break;
	}

	if (pos) {
		position = pos->frame;
	}

	// engine.timebase_callback (tstate, nframes, position, new_position);
}

int
JACKAudioBackend::_jack_sync_callback (jack_transport_state_t state, jack_position_t* pos, void* arg)
{
	return static_cast<JACKAudioBackend*> (arg)->jack_sync_callback (state, pos);
}

int
JACKAudioBackend::jack_sync_callback (jack_transport_state_t state, jack_position_t* pos)
{
	TransportState tstate;

	switch (state) {
	case JackTransportStopped:
		tstate = TransportStopped;
		break;
	case JackTransportRolling:
		tstate = TransportRolling;
		break;
	case JackTransportLooping:
		tstate = TransportLooping;
		break;
	case JackTransportStarting:
		tstate = TransportStarting;
		break;
	}

	return engine.sync_callback (tstate, pos->frame);

	return true;
}

int
JACKAudioBackend::_xrun_callback (void *arg)
{
	JACKAudioBackend* ae = static_cast<JACKAudioBackend*> (arg);
	if (ae->connected()) {
		ae->engine.Xrun (); /* EMIT SIGNAL */
	}
	return 0;
}

#ifdef HAVE_JACK_SESSION
void
JACKAudioBackend::_session_callback (jack_session_event_t *event, void *arg)
{
	JACKAudioBackend* ae = static_cast<JACKAudioBackend*> (arg);
	if (ae->connected()) {
		ae->engine.JackSessionEvent (event); /* EMIT SIGNAL */
	}
}
#endif

void
JACKAudioBackend::_freewheel_callback (int onoff, void *arg)
{
	static_cast<JACKAudioBackend*>(arg)->freewheel_callback (onoff);
}

void
JACKAudioBackend::freewheel_callback (int onoff)
{
	_freewheeling = onoff;
	engine.freewheel_callback (onoff);
}

void
JACKAudioBackend::_latency_callback (jack_latency_callback_mode_t mode, void* arg)
{
	return static_cast<JACKAudioBackend*> (arg)->jack_latency_callback (mode);
}

int
JACKAudioBackend::create_process_thread (boost::function<void()> f, pthread_t* thread, size_t stacksize)
{
        GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
        ThreadData* td = new ThreadData (this, f, stacksize);

        if (jack_client_create_thread (_priv_jack, thread, jack_client_real_time_priority (_priv_jack),
                                       jack_is_realtime (_priv_jack), _start_process_thread, td)) {
                return -1;
        }

        return 0;
}

void*
JACKAudioBackend::_start_process_thread (void* arg)
{
        ThreadData* td = reinterpret_cast<ThreadData*> (arg);
        boost::function<void()> f = td->f;
        delete td;

        f ();

        return 0;
}

void*
JACKAudioBackend::_process_thread (void *arg)
{
	return static_cast<JACKAudioBackend*> (arg)->process_thread ();
}

void*
JACKAudioBackend::process_thread ()
{
        /* JACK doesn't do this for us when we use the wait API
         */

        AudioEngine::thread_init_callback (this);

        while (1) {
                GET_PRIVATE_JACK_POINTER_RET(_priv_jack,0);

                pframes_t nframes = jack_cycle_wait (_priv_jack);
		
                if (engine.process_callback (nframes)) {
                        return 0;
                }

		jack_cycle_signal (_priv_jack, 0);
        }

        return 0;
}

int
JACKAudioBackend::_sample_rate_callback (pframes_t nframes, void *arg)
{
	return static_cast<JACKAudioBackend*> (arg)->jack_sample_rate_callback (nframes);
}

int
JACKAudioBackend::jack_sample_rate_callback (pframes_t nframes)
{
	_current_sample_rate = nframes;
	return engine.sample_rate_change (nframes);
}

void
JACKAudioBackend::jack_latency_callback (jack_latency_callback_mode_t mode)
{
	engine.latency_callback (mode == JackPlaybackLatency);
}

int
JACKAudioBackend::_bufsize_callback (pframes_t nframes, void *arg)
{
	return static_cast<JACKAudioBackend*> (arg)->jack_bufsize_callback (nframes);
}

int
JACKAudioBackend::jack_bufsize_callback (pframes_t nframes)
{
        /* if the size has not changed, this should be a no-op */

        if (nframes == _current_buffer_size) {
                return 0;
        }

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 1);

	_current_buffer_size = nframes;
	_current_usecs_per_cycle = (int) floor ((((double) nframes / sample_rate())) * 1000000.0);

        if (jack_port_type_get_buffer_size) {
                _raw_buffer_sizes[DataType::AUDIO] = jack_port_type_get_buffer_size (_priv_jack, JACK_DEFAULT_AUDIO_TYPE);
                _raw_buffer_sizes[DataType::MIDI] = jack_port_type_get_buffer_size (_priv_jack, JACK_DEFAULT_MIDI_TYPE);
        } else {

                /* Old version of JACK.

                   These crude guesses, see below where we try to get the right answers.

                   Note that our guess for MIDI deliberatey tries to overestimate
                   by a little. It would be nicer if we could get the actual
                   size from a port, but we have to use this estimate in the
                   event that there are no MIDI ports currently. If there are
                   the value will be adjusted below.
                */

                _raw_buffer_sizes[DataType::AUDIO] = nframes * sizeof (Sample);
                _raw_buffer_sizes[DataType::MIDI] = nframes * 4 - (nframes/2);
        }

	engine.buffer_size_change (nframes);

	return 0;
}

void
JACKAudioBackend::disconnected (const char* why)
{
        /* called from jack shutdown handler  */

	bool was_running = _running;

        _running = false;
        _current_buffer_size = 0;
        _current_sample_rate = 0;

        if (was_running) {
		engine.halted_callback (why); /* EMIT SIGNAL */
        }
}
float 
JACKAudioBackend::cpu_load() const 
{
	GET_PRIVATE_JACK_POINTER_RET(_priv_jack,0);
	return jack_cpu_load (_priv_jack);
}

void
JACKAudioBackend::update_latencies ()
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	jack_recompute_total_latencies (_priv_jack);
}

ChanCount
JACKAudioBackend::n_physical (unsigned long flags) const
{
	ChanCount c;

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, c);

	const char ** ports = jack_get_ports (_priv_jack, NULL, NULL, JackPortIsPhysical | flags);

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
			if (!strstr (ports[i], "Midi-Through")) {
				DataType t (jack_port_type (jack_port_by_name (_priv_jack, ports[i])));
				c.set (t, c.get (t) + 1);
			}
		}
		
		jack_free (ports);
	}

	return c;
}
