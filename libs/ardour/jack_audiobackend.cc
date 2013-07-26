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
#include <jack/jack.h>

#include <boost/scoped_ptr.hpp>

#include "pbd/epa.h"

#include "ardour/audioengine.h"
#include "ardour/types.h"
#include "ardour/jack_audiobackend.h"

using namespace ARDOUR;
using std::string;

#define GET_PRIVATE_JACK_POINTER(j)  jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(j,r) jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return r; }

extern "C" {

	/* functions looked up using dlopen-and-cousins, and so naming scope
	 * must be non-mangled.
	 */

	AudioBackend* backend_factory (AudioEngine& ae)
	{
		JACKAudioBackend* ab = new JACKAudioBackend (ae);
		return ab;
	}
	
}

JACKAudioBackend::JACKAudioBackend (AudioEngine& e)
	: AudioBackend (e)
	, _jack (0)
	, _target_sample_rate (48000)
	, _target_buffer_size (1024)
	, _target_sample_format (FloatingPoint)
	, _target_interleaved (false)
	, _target_input_channels (-1)
	, _target_output_channels (-1)
	, _target_systemic_input_latency (0)
	, _target_systemic_output_latency (0)
{
}

string
JACKAudioBackend::name() const 
{
	return X_("JACK");
}

void*
JACKAudioBackend::private_handle()
{
	return _jack;
}

bool
JACKAudioBackend::connected() const
{
	return (_jack != 0);
}

bool
JACKAudioBackend::is_realtime ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,false);
	return jack_is_realtime (_priv_jack);
}

vector<string>
JACKAudioBackend::enumerate_devices () const
{
	vector<string> devices;
	return devices;
}

vector<float>
JACKAudioBackend::available_sample_rates (const string& /*device*/) const
{
	vector<float> f;
	
	if (_jack) {
		f.push_back (get_sample_rate());
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
	
	if (_jack) {
		s.push_back (get_buffer_size());
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
JACKAudioBackend::available_input_channel_count (const string& /*device*/)
{
	return 128;
}

uint32_t
JACKAudioBackend::available_output_channel_count (const string& /*device*/)
{
	return 128;
}

/* -- parameter setting -- */

int
JACKAudioBackend::set_device_name (const string& dev)
{
	if (_jack) {
		/* need to stop and restart JACK for this to work, at present */
		return -1;
	}

	_target_device = dev;
	return 0;
}

int
JACKAudioBackend::set_sample_rate (float sr)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (sr == jack_get_sample_rate (_priv_jack)) {
                return 0;
	}

	return jack_set_sample_rate (_priv_jack, lrintf (sr));
}

int
JACKAudioBackend::set_buffer_size (uint32_t nframes)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

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
	if (sf == FloatingPoint) {
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
	if (_jack) {
		return -1;
	}

	_target_input_channels = cnt;
}

int
JACKAudioBackend::set_output_channels (uint32_t cnt)
{
	if (_jack) {
		return -1;
	}

	_target_output_channels = cnt;
}

int
JACKAudioBackend::set_systemic_input_latency (uint32_t l)
{
	if (_jack) {
		return -1;
	}

	_target_systemic_input_latency = l;
}

int
JACKAudioBackend::set_systemic_output_latency (uint32_t l)
{
	if (_jack) {
		return -1;
	}

	_target_systemic_output_latency = l;
}

/* --- Parameter retrieval --- */

std::string
JACKAudioBackend::get_device_name () const
{
}

float
JACKAudioBackend::get_sample_rate () const
{
	if (_jack) {
		return _current_sample_rate;
	}
	return _target_sample_rate;
}

uint32_t
JACKAudioBackend::get_buffer_size () const
{
	if (_jack) {
		return _current_buffer_size;
	}
	return _target_buffer_size;
}

SampleFormat
JACKAudioBackend::get_sample_format () const
{
	return FloatingPoint;
}

bool
JACKAudioBackend::get_interleaved () const
{
	return false;
}

uint32_t
JACKAudioBackend::get_input_channels () const
{
	if (_jack) {
		return n_physical (JackPortIsInput);
	} 
	return _target_input_channels;
}

uint32_t
JACKAudioBackend::get_output_channels () const
{
	if (_jack) {
		return n_physical (JackPortIsOutput);
	} 
	return _target_output_channels;
}

uint32_t
JACKAudioBackend::get_systemic_input_latency () const
{
	return _current_systemic_output_latency;
}

uint32_t
JACKAudioBackend::get_systemic_output_latency () const
{
	return _current_systemic_output_latency;
}

/* ---- BASIC STATE CONTROL API: start/stop/pause/freewheel --- */

int
JACKAudioBackend::start ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (!_running) {

                if (!jack_port_type_get_buffer_size) {
                        warning << _("This version of JACK is old - you should upgrade to a newer version that supports jack_port_type_get_buffer_size()") << endmsg;
		}

		if (_session) {
			BootMessage (_("Connect session to engine"));
			_session->set_frame_rate (jack_get_sample_rate (_priv_jack));
		}

                /* a proxy for whether jack_activate() will definitely call the buffer size
                 * callback. with older versions of JACK, this function symbol will be null.
                 * this is reliable, but not clean.
                 */

                if (!jack_port_type_get_buffer_size) {
			jack_bufsize_callback (jack_get_buffer_size (_priv_jack));
                }
		
		_processed_frames = 0;
		last_monitor_check = 0;

                set_jack_callbacks ();

		if (jack_activate (_priv_jack) == 0) {
			_running = true;
			_has_run = true;
			Running(); /* EMIT SIGNAL */
		} else {
			// error << _("cannot activate JACK client") << endmsg;
		}
	}
		
	return _running ? 0 : -1;
}

int
JACKAudioBackend::stop ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	{
		Glib::Threads::Mutex::Lock lm (_process_lock);
		jack_client_close (_priv_jack);
		_jack = 0;
	}

	_buffer_size = 0;
	_frame_rate = 0;
	_raw_buffer_sizes.clear();

	return 0;
}

int
JACKAudioBackend::pause ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (_priv_jack) {
		jack_deactivate (_priv_jack);
	}

	return 0;
}

int
JACKAudioBackend::freewheel (bool onoff)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (onoff == _freewheeling) {
		/* already doing what has been asked for */
		
		return 0;
	}

	return jack_set_freewheel (_priv_jack, onoff);
}

int
JACKAudioBackend::set_parameters (const Parameters& params)
{
	return 0;
}

int 
JACKAudioBackend::get_parameters (Parameters& params) const
{
	return 0;
}

/* parameters */

ARDOUR::pframes_t
AudioEngine::frames_per_cycle () const
{
 	GET_PRIVATE_JACK_POINTER_RET (_jack,0);
	if (_buffer_size == 0) {
		return jack_get_buffer_size (_jack);
	} else {
		return _buffer_size;
	}
}

ARDOUR::framecnt_t
AudioEngine::frame_rate () const
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, 0);
	if (_frame_rate == 0) {
		return (_frame_rate = jack_get_sample_rate (_priv_jack));
	} else {
		return _frame_rate;
	}
}

size_t
AudioEngine::raw_buffer_size (DataType t)
{
	std::map<DataType,size_t>::const_iterator s = _raw_buffer_sizes.find(t);
	return (s != _raw_buffer_sizes.end()) ? s->second : 0;
}



/*--- private support methods ---*/

int
JACKAudioBackend::connect_to_jack (string client_name, string session_uuid)
{
        EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
        boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;
	jack_status_t status;

        /* revert all environment settings back to whatever they were when ardour started
         */

        if (global_epa) {
                current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
                global_epa->restore ();
        }

	jack_client_name = client_name; /* might be reset below */
#ifdef HAVE_JACK_SESSION
	if (!session_uuid.empty())
	    _jack = jack_client_open (jack_client_name.c_str(), JackSessionID, &status, session_uuid.c_str());
	else
#endif
	_jack = jack_client_open (jack_client_name.c_str(), JackNullOption, &status, 0);

	if (_jack == NULL) {
		// error message is not useful here
		return -1;
	}

	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (status & JackNameNotUnique) {
		jack_client_name = jack_get_client_name (_priv_jack);
	}

	return 0;
}

int
JACKAudioBackend::disconnect_from_jack ()
{
}

int
JACKAudioBackend::reconnect_to_jack ()
{
	if (_running) {
		disconnect_from_jack ();
		/* XXX give jackd a chance */
		Glib::usleep (250000);
	}

	if (connect_to_jack (jack_client_name, "")) {
		error << _("failed to connect to JACK") << endmsg;
		return -1;
	}

	Ports::iterator i;

	boost::shared_ptr<Ports> p = ports.reader ();

	for (i = p->begin(); i != p->end(); ++i) {
		if (i->second->reestablish ()) {
			break;
		}
	}

	if (i != p->end()) {
		/* failed */
		remove_all_ports ();
		return -1;
	}

	GET_PRIVATE_JACK_POINTER_RET (_jack,-1);

	MIDI::Manager::instance()->reestablish (_priv_jack);

	if (_session) {
		_session->reset_jack_connection (_priv_jack);
                jack_bufsize_callback (jack_get_buffer_size (_priv_jack));
		_session->set_frame_rate (jack_get_sample_rate (_priv_jack));
	}

	last_monitor_check = 0;

        set_jack_callbacks ();

	if (jack_activate (_priv_jack) == 0) {
		_running = true;
		_has_run = true;
	} else {
		return -1;
	}

	/* re-establish connections */

	for (i = p->begin(); i != p->end(); ++i) {
		i->second->reconnect ();
	}

	MIDI::Manager::instance()->reconnect ();

	Running (); /* EMIT SIGNAL*/

	start_metering_thread ();

	return 0;
}

/* --- TRANSPORT STATE MANAGEMENT --- */

void
JACKAudioBackend::transport_stop ()
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_stop (_priv_jack);
}

void
JACKAudioBackend::transport_start ()
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_start (_priv_jack);
}

void
JACKAudioBackend::transport_locate (framepos_t where)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_locate (_priv_jack, where);
}

framepos_t 
JACKAudioBackend::transport_frame () const 
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, 0);
	return jack_get_current_transport_frame (_priv_jack);
}

JACKAudioBackend::TransportState
JACKAudioBackend::transport_state ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, ((TransportState) JackTransportStopped));
	jack_position_t pos;
	return (TransportState) jack_transport_query (_priv_jack, &pos);
}

int
JACKAudioBackend::set_time_master (bool yn)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);
	if (yn) {
		return jack_set_timebase_callback (_priv_jack, 0, _jack_timebase_callback, this);
	} else {
		return jack_release_timebase (_jack);
	}
}

/* process-time */

framecnt_t 
JACKAudioBackend::sample_rate () const
{

}
pframes_t 
JACKAudioBackend::samples_per_cycle () const
{
}

size_t 
JACKAudioBackend::raw_buffer_size(DataType t)
{
	std::map<DataType,size_t>::const_iterator s = _raw_buffer_sizes.find(t);
	return (s != _raw_buffer_sizes.end()) ? s->second : 0;
}

bool
JACKAudioBackend::get_sync_offset (pframes_t& offset) const
{

#ifdef HAVE_JACK_VIDEO_SUPPORT

	GET_PRIVATE_JACK_POINTER_RET (_jack, false);

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
JACKAudioBackend::frames_since_cycle_start ()
{
	jack_client_t* _priv_jack = _jack;
	if (!_running || !_priv_jack) {
		return 0;
	}
	return jack_frames_since_cycle_start (_priv_jack);
}

pframes_t
JACKAudioBackend::frame_time ()
{
	jack_client_t* _priv_jack = _jack;
	if (!_running || !_priv_jack) {
		return 0;
	}
	return jack_frame_time (_priv_jack);
}

pframes_t
JACKAudioBackend::frame_time_at_cycle_start ()
{
	jack_client_t* _priv_jack = _jack;
	if (!_running || !_priv_jack) {
		return 0;
	}
	return jack_last_frame_time (_priv_jack);
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
	GET_PRIVATE_JACK_POINTER (_jack);

        if (jack_on_info_shutdown) {
                jack_on_info_shutdown (_priv_jack, halted_info, this);
        } else {
                jack_on_shutdown (_priv_jack, halted, this);
        }

        jack_set_thread_init_callback (_priv_jack, _thread_init_callback, this);
        jack_set_process_thread (_priv_jack, _process_thread, this);
        jack_set_sample_rate_callback (_priv_jack, _sample_rate_callback, this);
        jack_set_buffer_size_callback (_priv_jack, _bufsize_callback, this);
        jack_set_graph_order_callback (_priv_jack, _graph_order_callback, this);
        jack_set_port_registration_callback (_priv_jack, _registration_callback, this);
        jack_set_port_connect_callback (_priv_jack, _connect_callback, this);
        jack_set_xrun_callback (_priv_jack, _xrun_callback, this);
        jack_set_sync_callback (_priv_jack, _jack_sync_callback, this);
        jack_set_freewheel_callback (_priv_jack, _freewheel_callback, this);

        if (_session && _session->config.get_jack_time_master()) {
                jack_set_timebase_callback (_priv_jack, 0, _jack_timebase_callback, this);
        }

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
	static_cast<AudioEngine*> (arg)->jack_timebase_callback (state, nframes, pos, new_position);
}

void
JACKAudioBackend::jack_timebase_callback (jack_transport_state_t state, pframes_t nframes,
				     jack_position_t* pos, int new_position)
{
	if (_jack && _session && _session->synced_to_jack()) {
		_session->jack_timebase_callback (state, nframes, pos, new_position);
	}
}

int
JACKAudioBackend::_jack_sync_callback (jack_transport_state_t state, jack_position_t* pos, void* arg)
{
	return static_cast<AudioEngine*> (arg)->jack_sync_callback (state, pos);
}

int
JACKAudioBackend::jack_sync_callback (jack_transport_state_t state, jack_position_t* pos)
{
	if (_jack && _session) {
		return _session->jack_sync_callback (state, pos);
	}

	return true;
}

int
JACKAudioBackend::_xrun_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->connected()) {
		ae->Xrun (); /* EMIT SIGNAL */
	}
	return 0;
}

#ifdef HAVE_JACK_SESSION
void
JACKAudioBackend::_session_callback (jack_session_event_t *event, void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->connected()) {
		ae->JackSessionEvent ( event ); /* EMIT SIGNAL */
	}
}
#endif

int
JACKAudioBackend::_graph_order_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);

	if (ae->connected() && !ae->port_remove_in_progress) {
		ae->GraphReordered (); /* EMIT SIGNAL */
	}
	
	return 0;
}

void
JACKAudioBackend::_freewheel_callback (int onoff, void *arg)
{
	static_cast<AudioEngine*>(arg)->freewheel_callback (onoff);
}

void
JACKAudioBackend::freewheel_callback (int onoff)
{
	_freewheeling = onoff;

	if (onoff) {
		_pre_freewheel_mmc_enabled = MIDI::Manager::instance()->mmc()->send_enabled ();
		MIDI::Manager::instance()->mmc()->enable_send (false);
	} else {
		MIDI::Manager::instance()->mmc()->enable_send (_pre_freewheel_mmc_enabled);
	}
}

void
JACKAudioBackend::_registration_callback (jack_port_id_t /*id*/, int /*reg*/, void* arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);

	if (!ae->port_remove_in_progress) {
		ae->PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
	}
}

void
JACKAudioBackend::_latency_callback (jack_latency_callback_mode_t mode, void* arg)
{
	return static_cast<AudioEngine *> (arg)->jack_latency_callback (mode);
}

void
JACKAudioBackend::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	ae->connect_callback (id_a, id_b, conn);
}

void
JACKAudioBackend::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
{
	if (port_remove_in_progress) {
		return;
	}

	GET_PRIVATE_JACK_POINTER (_jack);

	jack_port_t* jack_port_a = jack_port_by_id (_priv_jack, id_a);
	jack_port_t* jack_port_b = jack_port_by_id (_priv_jack, id_b);

	boost::shared_ptr<Port> port_a;
	boost::shared_ptr<Port> port_b;
	Ports::iterator x;
	boost::shared_ptr<Ports> pr = ports.reader ();

	x = pr->find (make_port_name_relative (jack_port_name (jack_port_a)));
	if (x != pr->end()) {
		port_a = x->second;
	}

	x = pr->find (make_port_name_relative (jack_port_name (jack_port_b)));
	if (x != pr->end()) {
		port_b = x->second;
	}

	PortConnectedOrDisconnected (
		port_a, jack_port_name (jack_port_a),
		port_b, jack_port_name (jack_port_b),
		conn == 0 ? false : true
		); /* EMIT SIGNAL */
}

int
JACKAudioBackend::create_process_thread (boost::function<void()> f, pthread_t* thread, size_t stacksize)
{
        GET_PRIVATE_JACK_POINTER_RET (_jack, 0);
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
	return static_cast<AudioEngine *> (arg)->process_thread ();
}

void*
JACKAudioBackend::process_thread ()
{
        /* JACK doesn't do this for us when we use the wait API
         */

        _thread_init_callback (0);

        _main_thread = new ProcessThread;

        while (1) {
                GET_PRIVATE_JACK_POINTER_RET(_jack,0);

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
	return static_cast<AudioEngine *> (arg)->jack_sample_rate_callback (nframes);
}

int
JACKAudioBackend::jack_sample_rate_callback (pframes_t nframes)
{
	_frame_rate = nframes;
	_usecs_per_cycle = (int) floor ((((double) frames_per_cycle() / nframes)) * 1000000.0);

	/* check for monitor input change every 1/10th of second */

	monitor_check_interval = nframes / 10;
	last_monitor_check = 0;

	if (_session) {
		_session->set_frame_rate (nframes);
	}

	SampleRateChanged (nframes); /* EMIT SIGNAL */

	return 0;
}

void
JACKAudioBackend::jack_latency_callback (jack_latency_callback_mode_t mode)
{
        if (_session) {
                _session->update_latency (mode == JackPlaybackLatency);
        }
}

int
JACKAudioBackend::_bufsize_callback (pframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->jack_bufsize_callback (nframes);
}

int
JACKAudioBackend::jack_bufsize_callback (pframes_t nframes)
{
        /* if the size has not changed, this should be a no-op */

        if (nframes == _current_buffer_size) {
                return 0;
        }

	GET_PRIVATE_JACK_POINTER_RET (_jack, 1);

	_current_buffer_size = nframes;
	_current_usecs_per_cycle = (int) floor ((((double) nframes / frame_rate())) * 1000000.0);
	last_monitor_check = 0;

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

	BufferSizeChange (nframes);

	if (_session) {
		_session->set_block_size (_buffer_size);
	}

	return 0;
}

void
JACKAudioBackend::halted_info (jack_status_t code, const char* reason, void *arg)
{
        /* called from jack shutdown handler  */

        AudioEngine* ae = static_cast<AudioEngine *> (arg);
        bool was_running = ae->_running;

        ae->stop_metering_thread ();

        ae->_running = false;
        ae->_buffer_size = 0;
        ae->_frame_rate = 0;
        ae->_jack = 0;

        if (was_running) {
		MIDI::JackMIDIPort::JackHalted (); /* EMIT SIGNAL */
#ifdef HAVE_JACK_ON_INFO_SHUTDOWN
                switch (code) {
                case JackBackendError:
                        ae->Halted(reason); /* EMIT SIGNAL */
                        break;
                default:
                        ae->Halted(""); /* EMIT SIGNAL */
                }
#else
                ae->Halted(""); /* EMIT SIGNAL */
#endif
        }
}

void
JACKAudioBackend::halted (void *arg)
{
        cerr << "HALTED by JACK\n";

        /* called from jack shutdown handler  */

	AudioEngine* ae = static_cast<AudioEngine *> (arg);
	bool was_running = ae->_running;

	ae->stop_metering_thread ();

	ae->_running = false;
	ae->_buffer_size = 0;
	ae->_frame_rate = 0;
        ae->_jack = 0;

	if (was_running) {
		MIDI::JackMIDIPort::JackHalted (); /* EMIT SIGNAL */
		ae->Halted(""); /* EMIT SIGNAL */
	}
}

