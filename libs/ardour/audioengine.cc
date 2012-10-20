/*
    Copyright (C) 2002 Paul Davis

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

#include <unistd.h>
#include <cerrno>
#include <vector>
#include <exception>
#include <stdexcept>
#include <sstream>

#include <glibmm/timer.h>

#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"
#include "pbd/unknown_type.h"
#include "pbd/epa.h"

#include <jack/weakjack.h>

#include "midi++/port.h"
#include "midi++/jack_midi_port.h"
#include "midi++/mmc.h"
#include "midi++/manager.h"

#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/buffer.h"
#include "ardour/cycle_timer.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/midi_port.h"
#include "ardour/port.h"
#include "ardour/process_thread.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

gint AudioEngine::m_meter_exit;
AudioEngine* AudioEngine::_instance = 0;

#define GET_PRIVATE_JACK_POINTER(j)  jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(j,r) jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return r; }

AudioEngine::AudioEngine (string client_name, string session_uuid)
	: _jack (0)
	, session_remove_pending (false)
	, session_removal_countdown (-1)
	, _running (false)
	, _has_run (false)
	, _buffer_size (0)
	, _frame_rate (0)
	, monitor_check_interval (INT32_MAX)
	, last_monitor_check (0)
	, _processed_frames (0)
	, _freewheeling (false)
	, _pre_freewheel_mmc_enabled (false)
	, _usecs_per_cycle (0)
	, port_remove_in_progress (false)
	, m_meter_thread (0)
	, _main_thread (0)
	, ports (new Ports)
{
	_instance = this; /* singleton */

	g_atomic_int_set (&m_meter_exit, 0);

	if (connect_to_jack (client_name, session_uuid)) {
		throw NoBackendAvailable ();
	}

	Port::set_engine (this);

#ifdef HAVE_LTC
	_ltc_input = register_port (DataType::AUDIO, _("LTC in"), true);

	/* As of October 2012, the LTC source port is the only thing that needs
	 * to care about Config parameters, so don't bother to listen if we're
	 * not doing LTC stuff. This might change if other parameters show up
	 * in the future that we need to care about with or without LTC.
	 */

	Config->ParameterChanged.connect_same_thread (config_connection, boost::bind (&AudioEngine::parameter_changed, this, _1));
#endif
}

AudioEngine::~AudioEngine ()
{
	config_connection.disconnect ();

	{
		Glib::Threads::Mutex::Lock tm (_process_lock);
		session_removed.signal ();

		if (_running) {
			jack_client_close (_jack);
			_jack = 0;
		}

		stop_metering_thread ();
	}
}

jack_client_t*
AudioEngine::jack() const
{
	return _jack;
}

void
_thread_init_callback (void * /*arg*/)
{
	/* make sure that anybody who needs to know about this thread
	   knows about it.
	*/

	pthread_set_name (X_("audioengine"));

	PBD::notify_gui_about_thread_creation ("gui", pthread_self(), X_("Audioengine"), 4096);
	PBD::notify_gui_about_thread_creation ("midiui", pthread_self(), X_("Audioengine"), 128);

	SessionEvent::create_per_thread_pool (X_("Audioengine"), 512);

	MIDI::JackMIDIPort::set_process_thread (pthread_self());
}

static void
ardour_jack_error (const char* msg)
{
	error << "JACK: " << msg << endmsg;
}

void
AudioEngine::set_jack_callbacks ()
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

int
AudioEngine::start ()
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

			reconnect_ltc ();

		} else {
			// error << _("cannot activate JACK client") << endmsg;
		}
	}
		
	return _running ? 0 : -1;
}

int
AudioEngine::stop (bool forever)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (_priv_jack) {
		if (forever) {
			disconnect_from_jack ();
		} else {
			jack_deactivate (_priv_jack);
			Stopped(); /* EMIT SIGNAL */
			MIDI::JackMIDIPort::JackHalted (); /* EMIT SIGNAL */
		}
	}

        if (forever) {
                stop_metering_thread ();
        }

	return _running ? -1 : 0;
}


bool
AudioEngine::get_sync_offset (pframes_t& offset) const
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

void
AudioEngine::_jack_timebase_callback (jack_transport_state_t state, pframes_t nframes,
				      jack_position_t* pos, int new_position, void *arg)
{
	static_cast<AudioEngine*> (arg)->jack_timebase_callback (state, nframes, pos, new_position);
}

void
AudioEngine::jack_timebase_callback (jack_transport_state_t state, pframes_t nframes,
				     jack_position_t* pos, int new_position)
{
	if (_jack && _session && _session->synced_to_jack()) {
		_session->jack_timebase_callback (state, nframes, pos, new_position);
	}
}

int
AudioEngine::_jack_sync_callback (jack_transport_state_t state, jack_position_t* pos, void* arg)
{
	return static_cast<AudioEngine*> (arg)->jack_sync_callback (state, pos);
}

int
AudioEngine::jack_sync_callback (jack_transport_state_t state, jack_position_t* pos)
{
	if (_jack && _session) {
		return _session->jack_sync_callback (state, pos);
	}

	return true;
}

int
AudioEngine::_xrun_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->connected()) {
		ae->Xrun (); /* EMIT SIGNAL */
	}
	return 0;
}

#ifdef HAVE_JACK_SESSION
void
AudioEngine::_session_callback (jack_session_event_t *event, void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->connected()) {
		ae->JackSessionEvent ( event ); /* EMIT SIGNAL */
	}
}
#endif

int
AudioEngine::_graph_order_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);

	if (ae->connected() && !ae->port_remove_in_progress) {
		ae->GraphReordered (); /* EMIT SIGNAL */
	}
	
	return 0;
}

void*
AudioEngine::_process_thread (void *arg)
{
	return static_cast<AudioEngine *> (arg)->process_thread ();
}

void
AudioEngine::_freewheel_callback (int onoff, void *arg)
{
	static_cast<AudioEngine*>(arg)->freewheel_callback (onoff);
}

void
AudioEngine::freewheel_callback (int onoff)
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
AudioEngine::_registration_callback (jack_port_id_t /*id*/, int /*reg*/, void* arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);

	if (!ae->port_remove_in_progress) {
		ae->PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
	}
}

void
AudioEngine::_latency_callback (jack_latency_callback_mode_t mode, void* arg)
{
	return static_cast<AudioEngine *> (arg)->jack_latency_callback (mode);
}

void
AudioEngine::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	ae->connect_callback (id_a, id_b, conn);
}

void
AudioEngine::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
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

void
AudioEngine::split_cycle (pframes_t offset)
{
	/* caller must hold process lock */

	Port::increment_global_port_buffer_offset (offset);

	/* tell all Ports that we're going to start a new (split) cycle */

	boost::shared_ptr<Ports> p = ports.reader();

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->cycle_split ();
	}
}

void*
AudioEngine::process_thread ()
{
        /* JACK doesn't do this for us when we use the wait API
         */

        _thread_init_callback (0);

        _main_thread = new ProcessThread;

        while (1) {
                GET_PRIVATE_JACK_POINTER_RET(_jack,0);

                pframes_t nframes = jack_cycle_wait (_priv_jack);

                if (process_callback (nframes)) {
                        return 0;
                }

		jack_cycle_signal (_priv_jack, 0);
        }

        return 0;
}

/** Method called by our ::process_thread when there is work to be done.
 *  @param nframes Number of frames to process.
 */
int
AudioEngine::process_callback (pframes_t nframes)
{
	GET_PRIVATE_JACK_POINTER_RET(_jack,0);
	Glib::Threads::Mutex::Lock tm (_process_lock, Glib::Threads::TRY_LOCK);

	PT_TIMING_REF;
	PT_TIMING_CHECK (1);

	/// The number of frames that will have been processed when we've finished
	pframes_t next_processed_frames;

	/* handle wrap around of total frames counter */

	if (max_framepos - _processed_frames < nframes) {
		next_processed_frames = nframes - (max_framepos - _processed_frames);
	} else {
		next_processed_frames = _processed_frames + nframes;
	}

	if (!tm.locked()) {
		/* return having done nothing */
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (session_remove_pending) {

		/* perform the actual session removal */

		if (session_removal_countdown < 0) {

			/* fade out over 1 second */
			session_removal_countdown = _frame_rate/2;
			session_removal_gain = 1.0;
			session_removal_gain_step = 1.0/session_removal_countdown;

		} else if (session_removal_countdown > 0) {

			/* we'll be fading audio out.
			   
			   if this is the last time we do this as part 
			   of session removal, do a MIDI panic now
			   to get MIDI stopped. This relies on the fact
			   that "immediate data" (aka "out of band data") from
			   MIDI tracks is *appended* after any other data, 
			   so that it emerges after any outbound note ons, etc.
			*/

			if (session_removal_countdown <= nframes) {
				_session->midi_panic ();
			}

		} else {
			/* fade out done */
			_session = 0;
			session_removal_countdown = -1; // reset to "not in progress"
			session_remove_pending = false;
			session_removed.signal(); // wakes up thread that initiated session removal
		}
	}

	if (_session == 0) {

		if (!_freewheeling) {
			MIDI::Manager::instance()->cycle_start(nframes);
			MIDI::Manager::instance()->cycle_end();
		}

		_processed_frames = next_processed_frames;

		return 0;
	}

	/* tell all relevant objects that we're starting a new cycle */

	InternalSend::CycleStart (nframes);
	Port::set_global_port_buffer_offset (0);
        Port::set_cycle_framecnt (nframes);

	/* tell all Ports that we're starting a new cycle */

	boost::shared_ptr<Ports> p = ports.reader();

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->cycle_start (nframes);
	}

	/* test if we are freewheeling and there are freewheel signals connected.
           ardour should act normally even when freewheeling unless /it/ is exporting */


	if (_freewheeling && !Freewheel.empty()) {
		/* emit the Freewheel signal and stop freewheeling in the event of trouble
		 */
                boost::optional<int> r = Freewheel (nframes);
		if (r.get_value_or (0)) {
			jack_set_freewheel (_priv_jack, false);
		}

	} else {
		MIDI::Manager::instance()->cycle_start(nframes);

		if (_session) {
			_session->process (nframes);
		}

		MIDI::Manager::instance()->cycle_end();
	}

	if (_freewheeling) {
		return 0;
	}

	if (!_running) {
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (last_monitor_check + monitor_check_interval < next_processed_frames) {

		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {

			bool x;

			if (i->second->last_monitor() != (x = i->second->jack_monitoring_input ())) {
				i->second->set_last_monitor (x);
				/* XXX I think this is dangerous, due to
				   a likely mutex in the signal handlers ...
				*/
				i->second->MonitorInputChanged (x); /* EMIT SIGNAL */
			}
		}
		last_monitor_check = next_processed_frames;
	}

	if (_session->silent()) {

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {

			if (i->second->sends_output()) {
				i->second->get_buffer(nframes).silence(nframes);
			}
		}
	}

	if (session_remove_pending && session_removal_countdown) {

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {

			if (i->second->sends_output()) {

				boost::shared_ptr<AudioPort> ap = boost::dynamic_pointer_cast<AudioPort> (i->second);
				if (ap) {
					Sample* s = ap->engine_get_whole_audio_buffer ();
					gain_t g = session_removal_gain;
					
					for (pframes_t n = 0; n < nframes; ++n) {
						*s++ *= g;
						g -= session_removal_gain_step;
					}
				}
			}
		}
		
		if (session_removal_countdown > nframes) {
			session_removal_countdown -= nframes;
		} else {
			session_removal_countdown = 0;
		}

		session_removal_gain -= (nframes * session_removal_gain_step);
	}

	// Finalize ports

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		i->second->cycle_end (nframes);
	}

	_processed_frames = next_processed_frames;

	PT_TIMING_CHECK (2);
	
	return 0;
}

int
AudioEngine::_sample_rate_callback (pframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->jack_sample_rate_callback (nframes);
}

int
AudioEngine::jack_sample_rate_callback (pframes_t nframes)
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
AudioEngine::jack_latency_callback (jack_latency_callback_mode_t mode)
{
        if (_session) {
                _session->update_latency (mode == JackPlaybackLatency);
        }
}

int
AudioEngine::_bufsize_callback (pframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->jack_bufsize_callback (nframes);
}

int
AudioEngine::jack_bufsize_callback (pframes_t nframes)
{
        /* if the size has not changed, this should be a no-op */

        if (nframes == _buffer_size) {
                return 0;
        }

	GET_PRIVATE_JACK_POINTER_RET (_jack, 1);

	_buffer_size = nframes;
	_usecs_per_cycle = (int) floor ((((double) nframes / frame_rate())) * 1000000.0);
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

	{
		Glib::Threads::Mutex::Lock lm (_process_lock);

		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			i->second->reset();
		}
	}

	if (_session) {
		_session->set_block_size (_buffer_size);
	}

	return 0;
}

void
AudioEngine::stop_metering_thread ()
{
	if (m_meter_thread) {
		g_atomic_int_set (&m_meter_exit, 1);
		m_meter_thread->join ();
		m_meter_thread = 0;
	}
}

void
AudioEngine::start_metering_thread ()
{
	if (m_meter_thread == 0) {
		g_atomic_int_set (&m_meter_exit, 0);
		m_meter_thread = Glib::Threads::Thread::create (boost::bind (&AudioEngine::meter_thread, this));
	}
}

void
AudioEngine::meter_thread ()
{
	pthread_set_name (X_("meter"));

	while (true) {
		Glib::usleep (10000); /* 1/100th sec interval */
		if (g_atomic_int_get(&m_meter_exit)) {
			break;
		}
		Metering::Meter ();
	}
}

void
AudioEngine::set_session (Session *s)
{
	Glib::Threads::Mutex::Lock pl (_process_lock);

	SessionHandlePtr::set_session (s);

	if (_session) {

		start_metering_thread ();

		pframes_t blocksize = jack_get_buffer_size (_jack);

		/* page in as much of the session process code as we
		   can before we really start running.
		*/

		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			i->second->cycle_start (blocksize);
		}

		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);
		_session->process (blocksize);

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			i->second->cycle_end (blocksize);
		}
	}
}

void
AudioEngine::remove_session ()
{
	Glib::Threads::Mutex::Lock lm (_process_lock);

	if (_running) {

		stop_metering_thread ();

		if (_session) {
			session_remove_pending = true;
			session_removed.wait(_process_lock);
		}

	} else {
		SessionHandlePtr::set_session (0);
	}

	remove_all_ports ();
}

void
AudioEngine::port_registration_failure (const std::string& portname)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	string full_portname = jack_client_name;
	full_portname += ':';
	full_portname += portname;


	jack_port_t* p = jack_port_by_name (_priv_jack, full_portname.c_str());
	string reason;

	if (p) {
		reason = string_compose (_("a port with the name \"%1\" already exists: check for duplicated track/bus names"), portname);
	} else {
		reason = string_compose (_("No more JACK ports are available. You will need to stop %1 and restart JACK with more ports if you need this many tracks."), PROGRAM_NAME);
	}

	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str());
}

boost::shared_ptr<Port>
AudioEngine::register_port (DataType dtype, const string& portname, bool input)
{
	boost::shared_ptr<Port> newport;

	try {
		if (dtype == DataType::AUDIO) {
			newport.reset (new AudioPort (portname, (input ? Port::IsInput : Port::IsOutput)));
		} else if (dtype == DataType::MIDI) {
			newport.reset (new MidiPort (portname, (input ? Port::IsInput : Port::IsOutput)));
		} else {
			throw PortRegistrationFailure("unable to create port (unknown type)");
		}

		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (make_pair (make_port_name_relative (portname), newport));

		/* writer goes out of scope, forces update */

		return newport;
	}

	catch (PortRegistrationFailure& err) {
		throw err;
	} catch (std::exception& e) {
		throw PortRegistrationFailure(string_compose(
				_("unable to create port: %1"), e.what()).c_str());
	} catch (...) {
		throw PortRegistrationFailure("unable to create port (unknown error)");
	}
}

boost::shared_ptr<Port>
AudioEngine::register_input_port (DataType type, const string& portname)
{
	return register_port (type, portname, true);
}

boost::shared_ptr<Port>
AudioEngine::register_output_port (DataType type, const string& portname)
{
	return register_port (type, portname, false);
}

int
AudioEngine::unregister_port (boost::shared_ptr<Port> port)
{
	/* caller must hold process lock */

	if (!_running) {
		/* probably happening when the engine has been halted by JACK,
		   in which case, there is nothing we can do here.
		   */
		return 0;
	}

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		Ports::iterator x = ps->find (make_port_name_relative (port->name()));

		if (x != ps->end()) {
			ps->erase (x);
		}

		/* writer goes out of scope, forces update */
	}

	ports.flush ();

	return 0;
}

int
AudioEngine::connect (const string& source, const string& destination)
{
	int ret;

	if (!_running) {
		if (!_has_run) {
			fatal << _("connect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	string s = make_port_name_non_relative (source);
	string d = make_port_name_non_relative (destination);


	boost::shared_ptr<Port> src = get_port_by_name (s);
	boost::shared_ptr<Port> dst = get_port_by_name (d);

	if (src) {
		ret = src->connect (d);
	} else if (dst) {
		ret = dst->connect (s);
	} else {
		/* neither port is known to us, and this API isn't intended for use as a general patch bay */
		ret = -1;
	}

	if (ret > 0) {
		/* already exists - no error, no warning */
	} else if (ret < 0) {
		error << string_compose(_("AudioEngine: cannot connect %1 (%2) to %3 (%4)"),
					source, s, destination, d)
		      << endmsg;
	}

	return ret;
}

int
AudioEngine::disconnect (const string& source, const string& destination)
{
	int ret;

	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	string s = make_port_name_non_relative (source);
	string d = make_port_name_non_relative (destination);

	boost::shared_ptr<Port> src = get_port_by_name (s);
	boost::shared_ptr<Port> dst = get_port_by_name (d);

	if (src) {
			ret = src->disconnect (d);
	} else if (dst) {
			ret = dst->disconnect (s);
	} else {
		/* neither port is known to us, and this API isn't intended for use as a general patch bay */
		ret = -1;
	}
	return ret;
}

int
AudioEngine::disconnect (boost::shared_ptr<Port> port)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,-1);

	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	return port->disconnect_all ();
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

/** @param name Full or short name of port
 *  @return Corresponding Port or 0.
 */

boost::shared_ptr<Port>
AudioEngine::get_port_by_name (const string& portname)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("get_port_by_name() called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			boost::shared_ptr<Port> ();
		}
	}

        if (!port_is_mine (portname)) {
                /* not an ardour port */
                return boost::shared_ptr<Port> ();
        }

	boost::shared_ptr<Ports> pr = ports.reader();
	std::string rel = make_port_name_relative (portname);
	Ports::iterator x = pr->find (rel);

	if (x != pr->end()) {
		/* its possible that the port was renamed by some 3rd party and
		   we don't know about it. check for this (the check is quick
		   and cheap), and if so, rename the port (which will alter
		   the port map as a side effect).
		*/
		const std::string check = make_port_name_relative (jack_port_name (x->second->jack_port()));
		if (check != rel) {
			x->second->set_name (check);
		}
		return x->second;
	}

        return boost::shared_ptr<Port> ();
}

void
AudioEngine::port_renamed (const std::string& old_relative_name, const std::string& new_relative_name)
{
	RCUWriter<Ports> writer (ports);
	boost::shared_ptr<Ports> p = writer.get_copy();
	Ports::iterator x = p->find (old_relative_name);
	
	if (x != p->end()) {
		boost::shared_ptr<Port> port = x->second;
		p->erase (x);
		p->insert (make_pair (new_relative_name, port));
	}
}

const char **
AudioEngine::get_ports (const string& port_name_pattern, const string& type_name_pattern, uint32_t flags)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,0);
	if (!_running) {
		if (!_has_run) {
			fatal << _("get_ports called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}
	return jack_get_ports (_priv_jack, port_name_pattern.c_str(), type_name_pattern.c_str(), flags);
}

void
AudioEngine::halted_info (jack_status_t code, const char* reason, void *arg)
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
AudioEngine::halted (void *arg)
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
		ae->Halted(""); /* EMIT SIGNAL */
		MIDI::JackMIDIPort::JackHalted (); /* EMIT SIGNAL */
	}
}

void
AudioEngine::died ()
{
        /* called from a signal handler for SIGPIPE */

	stop_metering_thread ();

        _running = false;
	_buffer_size = 0;
	_frame_rate = 0;
	_jack = 0;
}

bool
AudioEngine::can_request_hardware_monitoring ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,false);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortCanMonitor)) == 0) {
		return false;
	}

	free (ports);

	return true;
}

ChanCount
AudioEngine::n_physical (unsigned long flags) const
{
	ChanCount c;

	GET_PRIVATE_JACK_POINTER_RET (_jack, c);

	const char ** ports = jack_get_ports (_priv_jack, NULL, NULL, JackPortIsPhysical | flags);
	if (ports == 0) {
		return c;
	}

	for (uint32_t i = 0; ports[i]; ++i) {
		if (!strstr (ports[i], "Midi-Through")) {
			DataType t (jack_port_type (jack_port_by_name (_jack, ports[i])));
			c.set (t, c.get (t) + 1);
		}
	}

	free (ports);

	return c;
}

ChanCount
AudioEngine::n_physical_inputs () const
{
	return n_physical (JackPortIsInput);
}

ChanCount
AudioEngine::n_physical_outputs () const
{
	return n_physical (JackPortIsOutput);
}

void
AudioEngine::get_physical (DataType type, unsigned long flags, vector<string>& phy)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, type.to_jack_type(), JackPortIsPhysical | flags)) == 0) {
		return;
	}

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
                        if (strstr (ports[i], "Midi-Through")) {
                                continue;
                        }
			phy.push_back (ports[i]);
		}
		free (ports);
	}
}

/** Get physical ports for which JackPortIsOutput is set; ie those that correspond to
 *  a physical input connector.
 */
void
AudioEngine::get_physical_inputs (DataType type, vector<string>& ins)
{
	get_physical (type, JackPortIsOutput, ins);
}

/** Get physical ports for which JackPortIsInput is set; ie those that correspond to
 *  a physical output connector.
 */
void
AudioEngine::get_physical_outputs (DataType type, vector<string>& outs)
{
	get_physical (type, JackPortIsInput, outs);
}

void
AudioEngine::transport_stop ()
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_stop (_priv_jack);
}

void
AudioEngine::transport_start ()
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_start (_priv_jack);
}

void
AudioEngine::transport_locate (framepos_t where)
{
	GET_PRIVATE_JACK_POINTER (_jack);
	jack_transport_locate (_priv_jack, where);
}

AudioEngine::TransportState
AudioEngine::transport_state ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, ((TransportState) JackTransportStopped));
	jack_position_t pos;
	return (TransportState) jack_transport_query (_priv_jack, &pos);
}

int
AudioEngine::reset_timebase ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);
	if (_session) {
		if (_session->config.get_jack_time_master()) {
			return jack_set_timebase_callback (_priv_jack, 0, _jack_timebase_callback, this);
		} else {
			return jack_release_timebase (_jack);
		}
	}
	return 0;
}

int
AudioEngine::freewheel (bool onoff)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (onoff != _freewheeling) {
                return jack_set_freewheel (_priv_jack, onoff);

	} else {
                /* already doing what has been asked for */
                return 0;
	}
}

void
AudioEngine::remove_all_ports ()
{
	/* make sure that JACK callbacks that will be invoked as we cleanup
	 * ports know that they have nothing to do.
	 */

	port_remove_in_progress = true;

	/* process lock MUST be held by caller
	*/

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	/* clear dead wood list in RCU */

	ports.flush ();

	port_remove_in_progress = false;
}

int
AudioEngine::connect_to_jack (string client_name, string session_uuid)
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
	if (! session_uuid.empty())
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
AudioEngine::disconnect_from_jack ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, 0);

	if (_running) {
		stop_metering_thread ();
	}

	{
		Glib::Threads::Mutex::Lock lm (_process_lock);
		jack_client_close (_priv_jack);
		_jack = 0;
	}

	_buffer_size = 0;
	_frame_rate = 0;
	_raw_buffer_sizes.clear();

	if (_running) {
		_running = false;
		Stopped(); /* EMIT SIGNAL */
		MIDI::JackMIDIPort::JackHalted (); /* EMIT SIGNAL */
	}

	return 0;
}

int
AudioEngine::reconnect_to_jack ()
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

	reconnect_ltc ();

	Running (); /* EMIT SIGNAL*/

	start_metering_thread ();

	return 0;
}

int
AudioEngine::request_buffer_size (pframes_t nframes)
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (nframes == jack_get_buffer_size (_priv_jack)) {
                return 0;
	}

	return jack_set_buffer_size (_priv_jack, nframes);
}

string
AudioEngine::make_port_name_relative (string portname) const
{
	string::size_type len;
	string::size_type n;

	len = portname.length();

	for (n = 0; n < len; ++n) {
		if (portname[n] == ':') {
			break;
		}
	}

	if ((n != len) && (portname.substr (0, n) == jack_client_name)) {
		return portname.substr (n+1);
	}

	return portname;
}

string
AudioEngine::make_port_name_non_relative (string portname) const
{
	string str;

	if (portname.find_first_of (':') != string::npos) {
		return portname;
	}

	str  = jack_client_name;
	str += ':';
	str += portname;

	return str;
}

bool
AudioEngine::port_is_mine (const string& portname) const
{
	if (portname.find_first_of (':') != string::npos) {
		if (portname.substr (0, jack_client_name.length ()) != jack_client_name) {
                        return false;
                }
        }
        return true;
}

bool
AudioEngine::is_realtime () const
{
	GET_PRIVATE_JACK_POINTER_RET (_jack,false);
	return jack_is_realtime (_priv_jack);
}

int
AudioEngine::create_process_thread (boost::function<void()> f, pthread_t* thread, size_t stacksize)
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
AudioEngine::_start_process_thread (void* arg)
{
        ThreadData* td = reinterpret_cast<ThreadData*> (arg);
        boost::function<void()> f = td->f;
        delete td;

        f ();

        return 0;
}

bool
AudioEngine::port_is_physical (const std::string& portname) const
{
        GET_PRIVATE_JACK_POINTER_RET(_jack, false);

        jack_port_t *port = jack_port_by_name (_priv_jack, portname.c_str());

        if (!port) {
                return false;
        }

        return jack_port_flags (port) & JackPortIsPhysical;
}

void
AudioEngine::request_jack_monitors_input (const std::string& portname, bool yn) const
{
        GET_PRIVATE_JACK_POINTER(_jack);

        jack_port_t *port = jack_port_by_name (_priv_jack, portname.c_str());

        if (!port) {
                return;
        }

        jack_port_request_monitor (port, yn);
}

void
AudioEngine::update_latencies ()
{
        if (jack_recompute_total_latencies) {
                GET_PRIVATE_JACK_POINTER (_jack);
                jack_recompute_total_latencies (_priv_jack);
        }
}

void
AudioEngine::destroy ()
{
	delete _instance;
	_instance = 0;
}

void
AudioEngine::parameter_changed (const std::string& s)
{
	if (s == "ltc-source-port") {
		reconnect_ltc ();
	}

}

void
AudioEngine::reconnect_ltc ()
{
	if (_ltc_input) {

		string src = Config->get_ltc_source_port();

		_ltc_input->disconnect_all ();

		if (src != _("None") && !src.empty())  {
			_ltc_input->connect (src);
		}
	}
}
