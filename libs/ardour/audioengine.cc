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
#include <glibmm/pattern.h>
#include <glibmm/module.h>

#include "pbd/epa.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"
#include "pbd/unknown_type.h"

#include <jack/weakjack.h>

#include "midi++/port.h"
#include "midi++/jack_midi_port.h"
#include "midi++/mmc.h"
#include "midi++/manager.h"

#include "ardour/audio_port.h"
#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/backend_search_path.h"
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

AudioEngine::AudioEngine (const std::string& bcn, const std::string& bsu)
	: session_remove_pending (false)
	, session_removal_countdown (-1)
	, monitor_check_interval (INT32_MAX)
	, last_monitor_check (0)
	, _processed_frames (0)
	, _freewheeling (false)
	, _pre_freewheel_mmc_enabled (false)
	, _usecs_per_cycle (0)
	, port_remove_in_progress (false)
	, m_meter_thread (0)
	, _main_thread (0)
	, backend_client_name (bcn)
	, backend_session_uuid (bsu)
{
	g_atomic_int_set (&m_meter_exit, 0);
}

AudioEngine::~AudioEngine ()
{
	drop_backend ();

	config_connection.disconnect ();

	{
		Glib::Threads::Mutex::Lock tm (_process_lock);
		session_removed.signal ();
		stop_metering_thread ();
	}
}

AudioEngine*
AudioEngine::create (const std::string& bcn, const std::string& bsu)
{
	if (_instance) {
		return _instance;
	}
	return new AudioEngine (bcn, bsu);
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

int
AudioEngine::sample_rate_change (pframes_t nframes)
{
	/* check for monitor input change every 1/10th of second */

	monitor_check_interval = nframes / 10;
	last_monitor_check = 0;

	if (_session) {
		_session->set_frame_rate (nframes);
	}

	SampleRateChanged (nframes); /* EMIT SIGNAL */

	return 0;
}

int 
AudioEngine::buffer_size_change (pframes_t bufsiz)
{
	if (_session) {
		_session->set_block_size (bufsiz);
		last_monitor_check = 0;
	}

	return 0;
}

/** Method called by our ::process_thread when there is work to be done.
 *  @param nframes Number of frames to process.
 */
int
AudioEngine::process_callback (pframes_t nframes)
{
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
           ardour should act normally even when freewheeling unless /it/ is
           exporting 
	*/

	if (_freewheeling && !Freewheel.empty()) {

                Freewheel (nframes);

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

			if (i->second->last_monitor() != (x = i->second->monitoring_input ())) {
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

		pframes_t blocksize = _backend->buffer_size ();

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
AudioEngine::died ()
{
        /* called from a signal handler for SIGPIPE */

	stop_metering_thread ();

        _running = false;
	_buffer_size = 0;
	_frame_rate = 0;
}

int
AudioEngine::reset_timebase ()
{
	if (_session) {
		if (_session->config.get_jack_time_master()) {
			_backend->set_time_master (true);
		} else {
			_backend->set_time_master (false);
		}
	}
	return 0;
}


void
AudioEngine::destroy ()
{
	delete _instance;
	_instance = 0;
}

int
AudioEngine::discover_backends ()
{
	vector<std::string> backend_modules;

	_backends.clear ();

	Glib::PatternSpec so_extension_pattern("*.so");
	Glib::PatternSpec dylib_extension_pattern("*.dylib");

	find_matching_files_in_search_path (backend_search_path (),
	                                    so_extension_pattern, backend_modules);

	find_matching_files_in_search_path (backend_search_path (),
	                                    dylib_extension_pattern, backend_modules);

	DEBUG_TRACE (DEBUG::Panning, string_compose (_("looking for backends in %1"), backend_search_path().to_string()));

	for (vector<std::string>::iterator i = backend_modules.begin(); i != backend_modules.end(); ++i) {

		AudioBackendInfo* info;

		if ((info = backend_discover (*i)) != 0) {
			_backends.insert (make_pair (info->name, info));
		}
	}

	return _backends.size();
}

AudioBackendInfo*
AudioEngine::backend_discover (const string& path)
{
	Glib::Module* module = new Glib::Module(path);
	AudioBackendInfo* info;
	void* sym = 0;

	if (!module) {
		error << string_compose(_("AudioEngine: cannot load module \"%1\" (%2)"), path,
				Glib::Module::get_last_error()) << endmsg;
		delete module;
		return 0;
	}

	if (!module->get_symbol("descriptor", sym)) {
		error << string_compose(_("AudioEngine: backend at \"%1\" has no descriptor."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		delete module;
		return 0;
	}

	info = (AudioBackendInfo*) sym;

	return info;
}

vector<string>
AudioEngine::available_backends() const
{
	vector<string> r;
	
	for (BackendMap::const_iterator i = _backends.begin(); i != _backends.end(); ++i) {
		r.push_back (i->first);
	}

	return r;
}

string
AudioEngine::current_backend_name() const
{
	if (_backend) {
		return _backend->name();
	} 
	return string();
}

void
AudioEngine::drop_backend ()
{
	if (_backend) {
		_backend->stop ();
		_backend.reset ();
	}
}

int
AudioEngine::set_backend (const std::string& name)
{
	BackendMap::iterator b = _backends.find (name);

	if (b == _backends.end()) {
		return -1;
	}

	drop_backend ();

	try {

		_backend = b->second->backend_factory (*this);
		_impl = b->second->portengine_factory (*this);

	} catch (...) {
		error << string_compose (_("Could not create backend for %1"), name) << endmsg;
		return -1;
	}

	return 0;
}

/* BACKEND PROXY WRAPPERS */

int
AudioEngine::start ()
{
	if (!_backend) {
		return -1;
	}

	if (!_running) {

		if (_session) {
			BootMessage (_("Connect session to engine"));
			_session->set_frame_rate (_backend->sample_rate());
		}

		_processed_frames = 0;
		last_monitor_check = 0;

		if (_backend->start() == 0) {
			_running = true;
			_has_run = true;
			last_monitor_check = 0;

			if (_session && _session->config.get_jack_time_master()) {
				_backend->set_time_master (true);
			}

			Running(); /* EMIT SIGNAL */
		} else {
			/* should report error? */
		}
	}
		
	return _running ? 0 : -1;
}

int
AudioEngine::stop ()
{
	if (!_backend) {
		return 0;
	}

	Glib::Threads::Mutex::Lock lm (_process_lock);

	if (_backend->stop () == 0) {
		_running = false;
		stop_metering_thread ();

		Stopped (); /* EMIT SIGNAL */

		return 0;
	}

	return -1;
}

int
AudioEngine::pause ()
{
	if (!_backend) {
		return 0;
	}
	
	if (_backend->pause () == 0) {
		_running = false;
		Stopped(); /* EMIT SIGNAL */
		return 0;
	}

	return -1;
}

int
AudioEngine::freewheel (bool start_stop)
{
	if (!_backend) {
		return -1;
	}

	/* _freewheeling will be set when first Freewheel signal occurs */

	return _backend->freewheel (start_stop);
}

float
AudioEngine::get_cpu_load() const 
{
	if (!_backend) {
		return 0.0;
	}
	return _backend->cpu_load ();
}

bool
AudioEngine::is_realtime() const 
{
	if (!_backend) {
		return false;
	}

	return _backend->is_realtime();
}

bool
AudioEngine::connected() const 
{
	if (!_backend) {
		return false;
	}

	return _backend->connected();
}

void
AudioEngine::transport_start ()
{
	if (!_backend) {
		return;
	}
	return _backend->transport_start ();
}

void
AudioEngine::transport_stop ()
{
	if (!_backend) {
		return;
	}
	return _backend->transport_stop ();
}

TransportState
AudioEngine::transport_state ()
{
	if (!_backend) {
		return TransportStopped;
	}
	return _backend->transport_state ();
}

void
AudioEngine::transport_locate (framepos_t pos)
{
	if (!_backend) {
		return;
	}
	return _backend->transport_locate (pos);
}

framepos_t
AudioEngine::transport_frame()
{
	if (!_backend) {
		return 0;
	}
	return _backend->transport_frame ();
}

framecnt_t
AudioEngine::sample_rate () const
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_rate ();
}

pframes_t
AudioEngine::samples_per_cycle () const
{
	if (!_backend) {
		return 0;
	}
	return _backend->buffer_size ();
}

int
AudioEngine::usecs_per_cycle () const
{
	if (!_backend) {
		return -1;
	}
	return _backend->usecs_per_cycle ();
}

size_t
AudioEngine::raw_buffer_size (DataType t)
{
	if (!_backend) {
		return -1;
	}
	return _backend->raw_buffer_size (t);
}

pframes_t
AudioEngine::sample_time ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_time ();
}

pframes_t
AudioEngine::sample_time_at_cycle_start ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->sample_time_at_cycle_start ();
}

pframes_t
AudioEngine::samples_since_cycle_start ()
{
	if (!_backend) {
		return 0;
	}
	return _backend->samples_since_cycle_start ();
}

bool
AudioEngine::get_sync_offset (pframes_t& offset) const
{
	if (!_backend) {
		return false;
	}
	return _backend->get_sync_offset (offset);
}

int
AudioEngine::create_process_thread (boost::function<void()> func, pthread_t* thr, size_t stacksize)
{
	if (!_backend) {
		return -1;
	}
	return _backend->create_process_thread (func, thr, stacksize);
}


int
AudioEngine::set_device_name (const std::string& name)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_device_name  (name);
}

int
AudioEngine::set_sample_rate (float sr)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_sample_rate  (sr);
}

int
AudioEngine::set_buffer_size (uint32_t bufsiz)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_buffer_size  (bufsiz);
}

int
AudioEngine::set_sample_format (SampleFormat sf)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_sample_format  (sf);
}

int
AudioEngine::set_interleaved (bool yn)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_interleaved  (yn);
}

int
AudioEngine::set_input_channels (uint32_t ic)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_input_channels  (ic);
}

int
AudioEngine::set_output_channels (uint32_t oc)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_output_channels (oc);
}

int
AudioEngine::set_systemic_input_latency (uint32_t il)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_systemic_input_latency  (il);
}

int
AudioEngine::set_systemic_output_latency (uint32_t ol)
{
	if (!_backend) {
		return -1;
	}
	return _backend->set_systemic_output_latency  (ol);
}

/* END OF BACKEND PROXY API */

void
AudioEngine::thread_init_callback (void* arg)
{
	/* make sure that anybody who needs to know about this thread
	   knows about it.
	*/

	pthread_set_name (X_("audioengine"));

	PBD::notify_gui_about_thread_creation ("gui", pthread_self(), X_("AudioEngine"), 4096);
	PBD::notify_gui_about_thread_creation ("midiui", pthread_self(), X_("AudioEngine"), 128);

	SessionEvent::create_per_thread_pool (X_("AudioEngine"), 512);

	MIDI::JackMIDIPort::set_process_thread (pthread_self());

	if (arg) {
		AudioEngine* ae = static_cast<AudioEngine*> (arg);
		/* the special thread created/managed by the backend */
		ae->_main_thread = new ProcessThread;
	}
}

/* XXXX
void
AudioEngine::timebase_callback (TransportState state, pframes_t nframes, jack_position_t pos, int new_position)
{
	if (_session && _session->synced_to_jack()) {
		// _session->timebase_callback (state, nframes, pos, new_position);
	}
}
*/

int
AudioEngine::sync_callback (TransportState state, framepos_t position)
{
	if (_session) {
		return _session->jack_sync_callback (state, position);
	}
	return 0;
}

void
AudioEngine::freewheel_callback (bool onoff)
{
	if (onoff) {
		_pre_freewheel_mmc_enabled = MIDI::Manager::instance()->mmc()->send_enabled ();
		MIDI::Manager::instance()->mmc()->enable_send (false);
	} else {
		MIDI::Manager::instance()->mmc()->enable_send (_pre_freewheel_mmc_enabled);
	}
}

void
AudioEngine::latency_callback (bool for_playback)
{
        if (_session) {
                _session->update_latency (for_playback);
        }
}

void
AudioEngine::update_latencies ()
{
	if (_backend) {
		_backend->update_latencies ();
	}
}

void
AudioEngine::halted_callback (const char* why)
{
        stop_metering_thread ();
	
	MIDI::JackMIDIPort::EngineHalted (); /* EMIT SIGNAL */
	Halted (why); /* EMIT SIGNAL */
}
