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
#include <sstream>

#include <glibmm/timer.h>
#include <pbd/pthread_utils.h>
#include <pbd/stacktrace.h>

#include <ardour/audioengine.h>
#include <ardour/buffer.h>
#include <ardour/port.h>
#include <ardour/session.h>
#include <ardour/cycle_timer.h>
#include <ardour/utils.h>
#ifdef VST_SUPPORT
#include <fst.h>
#endif

#include <ardour/timestamps.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

gint AudioEngine::m_meter_exit;

static void 
ardour_jack_error (const char* msg) 
{
	// throw JACK errors away - they cause visual clutter
	// error << "JACK: " << msg << endmsg;
}

AudioEngine::AudioEngine (string client_name) 
	: ports (new Ports)
{
	session = 0;
	session_remove_pending = false;
	_running = false;
	_has_run = false;
	last_monitor_check = 0;
	monitor_check_interval = max_frames;
	_processed_frames = 0;
	_usecs_per_cycle = 0;
	_jack = 0;
	_frame_rate = 0;
	_buffer_size = 0;
	_freewheel_thread_registered = false;
	_freewheeling = false;

	m_meter_thread = 0;
	g_atomic_int_set (&m_meter_exit, 0);

	if (connect_to_jack (client_name)) {
		throw NoBackendAvailable ();
	}
}

AudioEngine::~AudioEngine ()
{
	{
		Glib::Mutex::Lock tm (_process_lock);
		session_removed.signal ();
		
		if (_running) {
			jack_client_close (_jack);
			_jack = 0;
		}
		
		stop_metering_thread ();
	}
}

void
_thread_init_callback (void *arg)
{
	/* make sure that anybody who needs to know about this thread
	   knows about it.
	*/

	PBD::ThreadCreatedWithRequestSize (pthread_self(), X_("Audioengine"), 4096);
}

int
AudioEngine::start ()
{
	if (!_jack) {
		error << _("AudioEngine::start() called while disconnected from JACK") << endmsg;
		return -1;
	}

	if (!_running) {

		if (session) {
			nframes_t blocksize = jack_get_buffer_size (_jack);

			BootMessage (_("Connect session to engine"));

			session->set_block_size (blocksize);
			session->set_frame_rate (jack_get_sample_rate (_jack));

			/* page in as much of the session process code as we
			   can before we really start running.
			*/

			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
			session->process (blocksize);
		}

		_processed_frames = 0;
		last_monitor_check = 0;

		jack_on_shutdown (_jack, halted, this);
		jack_set_graph_order_callback (_jack, _graph_order_callback, this);
		jack_set_thread_init_callback (_jack, _thread_init_callback, this);
		jack_set_process_callback (_jack, _process_callback, this);
		jack_set_sample_rate_callback (_jack, _sample_rate_callback, this);
		jack_set_buffer_size_callback (_jack, _bufsize_callback, this);
		jack_set_xrun_callback (_jack, _xrun_callback, this);
		jack_set_sync_callback (_jack, _jack_sync_callback, this);
		jack_set_freewheel_callback (_jack, _freewheel_callback, this);

		if (Config->get_jack_time_master()) {
			jack_set_timebase_callback (_jack, 0, _jack_timebase_callback, this);
		}

		if (jack_activate (_jack) == 0) {
			_running = true;
			_has_run = true;
			Running(); /* EMIT SIGNAL */
		} else {
			// error << _("cannot activate JACK client") << endmsg;
		}

		start_metering_thread();
	}

	return _running ? 0 : -1;
}

int
AudioEngine::stop (bool forever)
{
	if (_jack) {
		if (forever) {
			disconnect_from_jack ();
		} else {
			jack_deactivate (_jack);
			Stopped(); /* EMIT SIGNAL */
		}
	}

	return _running ? -1 : 0;
}


bool
AudioEngine::get_sync_offset (nframes_t& offset) const
{

#ifdef HAVE_JACK_VIDEO_SUPPORT

	jack_position_t pos;
	
	if (_jack) {
		(void) jack_transport_query (_jack, &pos);
		
		if (pos.valid & JackVideoFrameOffset) {
			offset = pos.video_offset;
			return true;
		}
	}

#endif

	return false;
}

void
AudioEngine::_jack_timebase_callback (jack_transport_state_t state, nframes_t nframes,
				      jack_position_t* pos, int new_position, void *arg)
{
	static_cast<AudioEngine*> (arg)->jack_timebase_callback (state, nframes, pos, new_position);
}

void
AudioEngine::jack_timebase_callback (jack_transport_state_t state, nframes_t nframes,
				     jack_position_t* pos, int new_position)
{
	if (_jack && session && session->synced_to_jack()) {
		session->jack_timebase_callback (state, nframes, pos, new_position);
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
	if (_jack && session) {
		return session->jack_sync_callback (state, pos);
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

int
AudioEngine::_graph_order_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->connected()) {
		ae->GraphReordered (); /* EMIT SIGNAL */
	}
	return 0;
}

int
AudioEngine::_process_callback (nframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->process_callback (nframes);
}

void
AudioEngine::_freewheel_callback (int onoff, void *arg)
{
	static_cast<AudioEngine*>(arg)->_freewheeling = onoff;
}

int
AudioEngine::process_callback (nframes_t nframes)
{
	// CycleTimer ct ("AudioEngine::process");
	Glib::Mutex::Lock tm (_process_lock, Glib::TRY_LOCK);
	nframes_t next_processed_frames;
	
	/* handle wrap around of total frames counter */

	if (max_frames - _processed_frames < nframes) {
		next_processed_frames = nframes - (max_frames - _processed_frames);
	} else {
		next_processed_frames = _processed_frames + nframes;
	}
	
	if (!tm.locked() || session == 0) {
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (session_remove_pending) {
		session = 0;
		session_remove_pending = false;
		session_removed.signal();
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (_freewheeling) {
		if (Freewheel (nframes)) {
			jack_set_freewheel (_jack, false);
		}
		return 0;
	}

	session->process (nframes);

	if (!_running) {
		/* we were zombified, maybe because a ladspa plugin took
		   too long, or jackd exited, or something like that.
		*/
		
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (last_monitor_check + monitor_check_interval < next_processed_frames) {

		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			
			Port *port = (*i);
			bool x;
			
			if (port->_last_monitor != (x = port->monitoring_input ())) {
				port->_last_monitor = x;
				/* XXX I think this is dangerous, due to 
				   a likely mutex in the signal handlers ...
				*/
				 port->MonitorInputChanged (x); /* EMIT SIGNAL */
			}
		}
		last_monitor_check = next_processed_frames;
	}

	if (session->silent()) {

		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			
			Port *port = (*i);
			
			if (port->sends_output()) {
				Sample *buf = port->get_buffer(nframes);
				memset (buf, 0, sizeof(Sample) * nframes);
				// this should work but doesn't
				//port->silence(0, nframes);  //need to implement declicking fade
			}
		}
	}

	_processed_frames = next_processed_frames;
	return 0;
}

int
AudioEngine::_sample_rate_callback (nframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->jack_sample_rate_callback (nframes);
}

int
AudioEngine::jack_sample_rate_callback (nframes_t nframes)
{
	_frame_rate = nframes;
	_usecs_per_cycle = (int) floor ((((double) frames_per_cycle() / nframes)) * 1000000.0);
	
	/* check for monitor input change every 1/10th of second */

	monitor_check_interval = nframes / 10;
	last_monitor_check = 0;
	
	if (session) {
		session->set_frame_rate (nframes);
	}

	SampleRateChanged (nframes); /* EMIT SIGNAL */

	return 0;
}

int
AudioEngine::_bufsize_callback (nframes_t nframes, void *arg)
{
	return static_cast<AudioEngine *> (arg)->jack_bufsize_callback (nframes);
}

int
AudioEngine::jack_bufsize_callback (nframes_t nframes)
{
	_buffer_size = nframes;
	_usecs_per_cycle = (int) floor ((((double) nframes / frame_rate())) * 1000000.0);
	last_monitor_check = 0;

	boost::shared_ptr<Ports> p = ports.reader();

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		(*i)->reset();
	}

	if (session) {
		session->set_block_size (_buffer_size);
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
		m_meter_thread = Glib::Thread::create (sigc::mem_fun(this, &AudioEngine::meter_thread),
						       500000, true, true, Glib::THREAD_PRIORITY_NORMAL);
	}
}

void
AudioEngine::meter_thread ()
{
	while (true) {
		Glib::usleep (10000); /* 1/100th sec interval */
		if (g_atomic_int_get(&m_meter_exit)) {
			break;
		}
		IO::update_meters ();
	}
}

void 
AudioEngine::set_session (Session *s)
{
	Glib::Mutex::Lock pl (_process_lock);

	if (!session) {

		session = s;

		nframes_t blocksize = jack_get_buffer_size (_jack);
		
		/* page in as much of the session process code as we
		   can before we really start running.
		*/
		
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
		session->process (blocksize);
	}
}

void 
AudioEngine::remove_session ()
{
	Glib::Mutex::Lock lm (_process_lock);

	if (_running) {

		if (session) {
			session_remove_pending = true;
			session_removed.wait(_process_lock);
		}

	} else {
		session = 0;
	}
	
	remove_all_ports ();
}

void
AudioEngine::port_registration_failure (const std::string& portname)
{
	string full_portname = jack_client_name;
	full_portname += ':';
	full_portname += portname;
	
	
	jack_port_t* p = jack_port_by_name (_jack, full_portname.c_str());
	string reason;
	
	if (p) {
		reason = _("a port with this name already exists: check for duplicated track/bus names");
	} else {
		reason = _("unknown error");
	}
	
	throw PortRegistrationFailure (string_compose (_("AudioEngine: cannot register port \"%1\": %2"), portname, reason).c_str());
}	

Port *
AudioEngine::register_input_port (DataType type, const string& portname)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("register input port called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}

	jack_port_t *p = jack_port_register (_jack, portname.c_str(), type.to_jack_type(), JackPortIsInput, 0);

	if (p) {

		Port *newport;

		if ((newport = new Port (p)) != 0) {
			RCUWriter<Ports> writer (ports);
			boost::shared_ptr<Ports> ps = writer.get_copy ();
			ps->insert (ps->begin(), newport);
			/* writer goes out of scope, forces update */
		}

		return newport;

	} else {
		port_registration_failure (portname);
	}

	return 0;
}

Port *
AudioEngine::register_output_port (DataType type, const string& portname)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("register output port called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}

	jack_port_t *p;

	if ((p = jack_port_register (_jack, portname.c_str(),
		type.to_jack_type(), JackPortIsOutput, 0)) != 0) {

		Port *newport = 0;

		{
			RCUWriter<Ports> writer (ports);
			boost::shared_ptr<Ports> ps = writer.get_copy ();
			
			newport = new Port (p);
			ps->insert (ps->begin(), newport);

			/* writer goes out of scope, forces update */
		}

		return newport;

	} else {
		port_registration_failure (portname);
	}

	return 0;
}


int          
AudioEngine::unregister_port (Port *port)
{
	if (!_running) { 
		/* probably happening when the engine has been halted by JACK,
		   in which case, there is nothing we can do here.
		*/
		return 0;
	}

	if (port) {

		int ret = jack_port_unregister (_jack, port->_port);
		
		if (ret == 0) {
			
			{

				RCUWriter<Ports> writer (ports);
				boost::shared_ptr<Ports> ps = writer.get_copy ();
				
				for (Ports::iterator i = ps->begin(); i != ps->end(); ++i) {
					if ((*i) == port) {
						ps->erase (i);
						break;
					}
				}

				/* writer goes out of scope, forces update */
			}

			remove_connections_for (port);
		}

		return ret;

	} else {
		return -1;
	}
}

int 
AudioEngine::connect (const string& source, const string& destination)
{
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

	int ret = jack_connect (_jack, s.c_str(), d.c_str());

	if (ret == 0) {
		pair<string,string> c (s, d);
		port_connections.push_back (c);
	} else if (ret == EEXIST) {
		error << string_compose(_("AudioEngine: connection already exists: %1 (%2) to %3 (%4)"), 
				 source, s, destination, d) 
		      << endmsg;
	} else {
		error << string_compose(_("AudioEngine: cannot connect %1 (%2) to %3 (%4)"), 
				 source, s, destination, d) 
		      << endmsg;
	}

	return ret;
}

int 
AudioEngine::disconnect (const string& source, const string& destination)
{
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

	int ret = jack_disconnect (_jack, s.c_str(), d.c_str());

	if (ret == 0) {
		pair<string,string> c (s, d);
		PortConnections::iterator i;
		
		if ((i = find (port_connections.begin(), port_connections.end(), c)) != port_connections.end()) {
			port_connections.erase (i);
		}
	}
	 
	return ret;
}

int
AudioEngine::disconnect (Port *port)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	int ret = jack_port_disconnect (_jack, port->_port);

	if (ret == 0) {
		remove_connections_for (port);
	}

	return ret;

}

nframes_t
AudioEngine::frame_rate ()
{
	if (_jack) {
		if (_frame_rate == 0) {
			return (_frame_rate = jack_get_sample_rate (_jack));
		} else {
			return _frame_rate;
		}
	} else {
		fatal << X_("programming error: AudioEngine::frame_rate() called while disconnected from JACK")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}

nframes_t
AudioEngine::frames_per_cycle ()
{
	if (_jack) {
		if (_buffer_size == 0) {
			return (_buffer_size = jack_get_buffer_size (_jack));
		} else {
			return _buffer_size;
		}
	} else {
		fatal << X_("programming error: AudioEngine::frame_rate() called while disconnected from JACK")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}

Port *
AudioEngine::get_port_by_name (const string& portname, bool keep)
{
	Glib::Mutex::Lock lm (_process_lock);

	if (!_running) {
		if (!_has_run) {
			fatal << _("get_port_by_name() called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}
	
	/* check to see if we have a Port for this name already */

	boost::shared_ptr<Ports> pr = ports.reader();
	
	for (Ports::iterator i = pr->begin(); i != pr->end(); ++i) {
		if (portname == (*i)->name()) {
			return (*i);
		}
	}

	jack_port_t *p;

	if ((p = jack_port_by_name (_jack, portname.c_str())) != 0) {
		Port *newport = new Port (p);

		{
			if (keep && newport->is_mine (_jack)) {
				RCUWriter<Ports> writer (ports);
				boost::shared_ptr<Ports> ps = writer.get_copy ();
				ps->insert (newport);
				/* writer goes out of scope, forces update */
			}
		}

		return newport;

	} else {

		return 0;
	}
}

const char **
AudioEngine::get_ports (const string& port_name_pattern, const string& type_name_pattern, uint32_t flags)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("get_ports called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return 0;
		}
	}
	return jack_get_ports (_jack, port_name_pattern.c_str(), type_name_pattern.c_str(), flags);
}

void
AudioEngine::halted (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine *> (arg);
	bool was_running = ae->_running;

	ae->stop_metering_thread ();

	ae->_running = false;
	ae->_buffer_size = 0;
	ae->_frame_rate = 0;

	cerr << "!!! HALTED !!!\n";

	if (was_running) {
		ae->Halted(); /* EMIT SIGNAL */
	}
}

bool
AudioEngine::can_request_hardware_monitoring () 
{
	const char ** ports;

	if (!_jack) {
		return 0;
	}

	if ((ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortCanMonitor)) == 0) {
		return false;
	}

	free (ports);

	return true;
}


uint32_t
AudioEngine::n_physical_audio_outputs () const
{
	const char ** ports;
	uint32_t i = 0;

	if (!_jack) {
		return 0;
	}

	if ((ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|JackPortIsInput)) == 0) {
		return 0;
	}

	for (i = 0; ports[i]; ++i);
	free (ports);

	return i;
}

uint32_t
AudioEngine::n_physical_audio_inputs () const
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return 0;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|JackPortIsOutput)) == 0) {
		return 0;
	}

	if (ports) {
		for (i = 0; ports[i]; ++i);
		free (ports);
	}
	return i;
}

void
AudioEngine::get_physical_audio_inputs (vector<string>& ins)
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|JackPortIsOutput)) == 0) {
		return;
	}

	if (ports) {
		for (i = 0; ports[i]; ++i) {
			ins.push_back (ports[i]);
		}
		free (ports);
	}
}

void
AudioEngine::get_physical_audio_outputs (vector<string>& outs)
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|JackPortIsInput)) == 0) {
		return;
	}

	if (ports) {
		for (i = 0; ports[i]; ++i) {
			outs.push_back (ports[i]);
		}
		free (ports);
	}
}

string
AudioEngine::get_nth_physical_audio (uint32_t n, int flag)
{
	const char ** ports;
	uint32_t i;
	string ret;

	if (!_jack) {
		fatal << _("get_nth_physical called before engine was connected") << endmsg;
		/*NOTREACHED*/
	}

	ports = jack_get_ports (_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|flag);
	
	if (ports == 0) {
		return "";
	}

	for (i = 0; i < n && ports[i]; ++i);

	if (ports[i]) {
		ret = ports[i];
	}

	free ((char *) ports);

	return ret;
}

nframes_t
AudioEngine::get_port_total_latency (const Port& port)
{
	if (!_jack) {
		fatal << _("get_port_total_latency() called with no JACK client connection") << endmsg;
		/*NOTREACHED*/
	}

	return jack_port_get_total_latency (_jack, port._port);
}

void
AudioEngine::transport_stop ()
{
	if (_jack) {
		jack_transport_stop (_jack);
	}
}

void
AudioEngine::transport_start ()
{
	// cerr << "tell JACK to start\n";
	if (_jack) {
		jack_transport_start (_jack);
	}
}

void
AudioEngine::transport_locate (nframes_t where)
{
	// cerr << "tell JACK to locate to " << where << endl;
	if (_jack) {
		jack_transport_locate (_jack, where);
	}
}

AudioEngine::TransportState
AudioEngine::transport_state ()
{
	if (_jack) {
		jack_position_t pos;
		return (TransportState) jack_transport_query (_jack, &pos);
	} else {
		return (TransportState) JackTransportStopped;
	}
}

int
AudioEngine::reset_timebase ()
{
	if (_jack) {
		if (Config->get_jack_time_master()) {
			return jack_set_timebase_callback (_jack, 0, _jack_timebase_callback, this);
		} else {
			return jack_release_timebase (_jack);
		}
	} else {
		return -1;
	}
}

int
AudioEngine::freewheel (bool onoff)
{
	if (_jack) {

		if (onoff != _freewheeling) {

			if (onoff) {
				_freewheel_thread_registered = false;
			}

			return jack_set_freewheel (_jack, onoff);

		} else {
			/* already doing what has been asked for */
			return 0;
		}

	} else {
		return -1;
	}
}

void
AudioEngine::remove_all_ports ()
{
	/* process lock MUST be held */

	if (_jack) {
		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			jack_port_unregister (_jack, (*i)->_port);
		}
	}

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}

	port_connections.clear ();
}

void
AudioEngine::remove_connections_for (Port* port)
{
	for (PortConnections::iterator i = port_connections.begin(); i != port_connections.end(); ) {
		PortConnections::iterator tmp;
		
		tmp = i;
		++tmp;
		
		if ((*i).first == port->name()) {
			port_connections.erase (i);
		}

		i = tmp;
	}
}

#ifdef HAVE_JACK_CLIENT_OPEN

int
AudioEngine::connect_to_jack (string client_name)
{
	jack_options_t options = JackNullOption;
	jack_status_t status;
	const char *server_name = NULL;

	jack_client_name = client_name; /* might be reset below */
	_jack = jack_client_open (jack_client_name.c_str(), options, &status, server_name);

	if (_jack == NULL) {
		/* just return without an error message. something else will take care of it */
		return -1;
	}

	if (status & JackNameNotUnique) {
		jack_client_name = jack_get_client_name (_jack);
	}

	jack_set_error_function (ardour_jack_error);
	
	return 0;
}

#else

int
AudioEngine::connect_to_jack (string client_name)
{
	jack_client_name = client_name;

	if ((_jack = jack_client_new (client_name.c_str())) == 0) {
		return -1;
	}

	return 0;
}

#endif /* HAVE_JACK_CLIENT_OPEN */

int 
AudioEngine::disconnect_from_jack ()
{
	if (!_jack) {
		return 0;
	}


	if (_running) {
		stop_metering_thread ();
	}

	{ 
		Glib::Mutex::Lock lm (_process_lock);
		jack_client_close (_jack);
		_jack = 0;
	}

	_buffer_size = 0;
	_frame_rate = 0;

	if (_running) {
		_running = false;
		Stopped(); /* EMIT SIGNAL */
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

	if (connect_to_jack (jack_client_name)) {
		error << _("failed to connect to JACK") << endmsg;
		return -1;
	}

	Ports::iterator i;

	boost::shared_ptr<Ports> p = ports.reader ();

	for (i = p->begin(); i != p->end(); ++i) {

		/* XXX hack hack hack */

		string long_name = (*i)->name();
		string short_name;
		
		short_name = long_name.substr (long_name.find_last_of (':') + 1);

		if (((*i)->_port = jack_port_register (_jack, short_name.c_str(), (*i)->type(), (*i)->flags(), 0)) == 0) {
			error << string_compose (_("could not reregister %1"), (*i)->name()) << endmsg;
			break;
		} else {
		}

		(*i)->reset ();

		if ((*i)->flags() & JackPortIsOutput) {
			(*i)->silence (jack_get_buffer_size (_jack), 0);
		}
	}

	if (i != p->end()) {
		/* failed */
		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			jack_port_unregister (_jack, (*i)->_port);
		}
		return -1;
	} 


	if (session) {
		session->reset_jack_connection (_jack);
		nframes_t blocksize = jack_get_buffer_size (_jack);
		session->set_block_size (blocksize);
		session->set_frame_rate (jack_get_sample_rate (_jack));
	}

	last_monitor_check = 0;
	
	jack_on_shutdown (_jack, halted, this);
	jack_set_graph_order_callback (_jack, _graph_order_callback, this);
	jack_set_thread_init_callback (_jack, _thread_init_callback, this);
	jack_set_process_callback (_jack, _process_callback, this);
	jack_set_sample_rate_callback (_jack, _sample_rate_callback, this);
	jack_set_buffer_size_callback (_jack, _bufsize_callback, this);
	jack_set_xrun_callback (_jack, _xrun_callback, this);
	jack_set_sync_callback (_jack, _jack_sync_callback, this);
	jack_set_freewheel_callback (_jack, _freewheel_callback, this);
	
	if (Config->get_jack_time_master()) {
		jack_set_timebase_callback (_jack, 0, _jack_timebase_callback, this);
	}
	
	if (jack_activate (_jack) == 0) {
		_running = true;
		_has_run = true;
	} else {
		return -1;
	}

	/* re-establish connections */
	
	for (PortConnections::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {
		
		int err;
		jack_client_t* j = _jack;

		/* JACK could have zombified us. */

		if (!j) {
			error << _("Disconnected from JACK while reconnecting. You should quit Ardour now.") << endmsg;
			return -1;
		}
		
		if ((err = jack_connect (j, (*i).first.c_str(), (*i).second.c_str())) != 0) {
			if (err != EEXIST) {
				error << string_compose (_("could not reconnect %1 and %2 (err = %3)"),
						  (*i).first, (*i).second, err)
				      << endmsg;
			}
		}
	}

	Running (); /* EMIT SIGNAL*/

	start_metering_thread ();

	return 0;
}

int
AudioEngine::request_buffer_size (nframes_t nframes)
{
	if (_jack) {

		if (nframes == jack_get_buffer_size (_jack)) {
			return 0;
		}

		return jack_set_buffer_size (_jack, nframes);

	} else {
		return -1;
	}
}

void
AudioEngine::update_total_latencies ()
{
#ifdef HAVE_JACK_RECOMPUTE_LATENCIES
	if (_jack) {
		jack_recompute_total_latencies (_jack);
	}
#endif
}
		
string
AudioEngine::make_port_name_relative (string portname)
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
AudioEngine::make_port_name_non_relative (string portname)
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
AudioEngine::is_realtime () const
{
	if (_jack) {
		return jack_is_realtime (_jack);
	} else {
		return false;
	}
}
