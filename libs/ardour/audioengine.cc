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
#include <pbd/pthread_utils.h>
#include <pbd/stacktrace.h>
#include <pbd/unknown_type.h>

#include <ardour/audioengine.h>
#include <ardour/buffer.h>
#include <ardour/port.h>
#include <ardour/jack_audio_port.h>
#include <ardour/jack_midi_port.h>
#include <ardour/audio_port.h>
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
	error << "JACK: " << msg << endmsg;
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
	_freewheeling = false;
	_usecs_per_cycle = 0;
	_jack = 0;
	_frame_rate = 0;
	_buffer_size = 0;
	_freewheeling = false;
	_freewheel_thread_registered = false;

	m_meter_thread = 0;
	g_atomic_int_set (&m_meter_exit, 0);

	if (connect_to_jack (client_name)) {
		throw NoBackendAvailable ();
	}
	Port::set_engine (this);
}

AudioEngine::~AudioEngine ()
{
	{
		Glib::Mutex::Lock tm (_process_lock);
		session_removed.signal ();
		
		if (_running) {
			jack_client_close (_jack);
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
	if (_running) {
		_running = false;
		stop_metering_thread ();
		if (forever) {
			jack_client_t* foo = _jack;
			_jack = 0;
			jack_client_close (foo);
		} else {
			jack_deactivate (_jack);
		}
		Stopped(); /* EMIT SIGNAL */
	}

	return _running ? -1 : 0;
}


bool
AudioEngine::get_sync_offset (nframes_t& offset) const
{

#ifdef HAVE_JACK_VIDEO_SUPPORT

	jack_position_t pos;
	
	(void) jack_transport_query (_jack, &pos);

	if (pos.valid & JackVideoFrameOffset) {
		offset = pos.video_offset;
		return true;
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
	if (ae->jack()) {
		ae->Xrun (); /* EMIT SIGNAL */
	}
	return 0;
}

int
AudioEngine::_graph_order_callback (void *arg)
{
	AudioEngine* ae = static_cast<AudioEngine*> (arg);
	if (ae->jack()) {
		ae->GraphReordered (); /* EMIT SIGNAL */
	}
	return 0;
}

/** Wrapped which is called by JACK as its process callback.  It is just
 * here to get us back into C++ land by calling AudioEngine::process_callback()
 * @param nframes Number of frames passed by JACK.
 * @param arg User argument passed by JACK, which will be the AudioEngine*.
 */
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

/** Method called by JACK (via _process_callback) which says that there
 * is work to be done.
 * @param nframes Number of frames to process.
 */
int
AudioEngine::process_callback (nframes_t nframes)
{
	// CycleTimer ct ("AudioEngine::process");
	Glib::Mutex::Lock tm (_process_lock, Glib::TRY_LOCK);

	/// The number of frames that will have been processed when we've finished
	nframes_t next_processed_frames;
	
	/* handle wrap around of total frames counter */

	if (max_frames - _processed_frames < nframes) {
		next_processed_frames = nframes - (max_frames - _processed_frames);
	} else {
		next_processed_frames = _processed_frames + nframes;
	}

	if (!tm.locked() || session == 0) {
		/* return having done nothing */
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (session_remove_pending) {
		/* perform the actual session removal */
		session = 0;
		session_remove_pending = false;
		session_removed.signal();
		_processed_frames = next_processed_frames;
		return 0;
	}

	if (_freewheeling) {
		/* emit the Freewheel signal and stop freewheeling in the event of trouble */
		if (Freewheel (nframes)) {
			cerr << "Freewheeling returned non-zero!\n";
			_freewheeling = false;
			jack_set_freewheel (_jack, false);
		}
		return 0;
	}

	boost::shared_ptr<Ports> p = ports.reader();

	// Prepare ports (ie read data if necessary)
	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		(*i)->cycle_start (nframes, 0);
	}
	
	if (session) {
		session->process (nframes);
	}
	
	// Finalize ports (ie write data if necessary)

	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		(*i)->cycle_end (nframes, 0);
	}

	if (!_running) {
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
				port->get_buffer().silence(nframes);
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
		
		boost::shared_ptr<Ports> p = ports.reader();

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			(*i)->cycle_start (blocksize, 0);
		}

		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);
		s->process (blocksize);

		for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
			(*i)->cycle_end (blocksize, 0);
		}
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
AudioEngine::register_port (DataType dtype, const string& portname, bool input, bool publish)
{
	Port* newport = 0;

	try {
		if (dtype == DataType::AUDIO) {
			newport = new AudioPort (portname, (input ? Port::IsInput : Port::IsOutput), publish, frames_per_cycle());
		} else if (dtype == DataType::MIDI) {
			newport = new MidiPort (portname, (input ? Port::IsInput : Port::IsOutput), publish, frames_per_cycle());
		} else {
			throw unknown_type();
		}

		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->insert (ps->begin(), newport);
		/* writer goes out of scope, forces update */

		return newport;
	}

	catch (...) {
		throw PortRegistrationFailure("unable to create port (unknown type?)");
	}
}

Port*
AudioEngine::get_port (const std::string& full_name)
{
	boost::shared_ptr<Ports> p = ports.reader();
	
	for (Ports::iterator i = p->begin(); i != p->end(); ++i) {
		if ((*i)->name() == full_name) {
			return *i;
		}
	}
	return 0;
}


Port *
AudioEngine::register_input_port (DataType type, const string& portname, bool publish)
{
	return register_port (type, portname, true, publish);
}

Port *
AudioEngine::register_output_port (DataType type, const string& portname, bool publish)
{
	return register_port (type, portname, false, publish);
}

int
AudioEngine::unregister_port (Port& port)
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
		
		for (Ports::iterator i = ps->begin(); i != ps->end(); ++i) {
			if ((*i) == &port) {
				delete *i;
				ps->erase (i);
				break;
			}
		}
		
		/* writer goes out of scope, forces update */
	}
		
	remove_connections_for (port);

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
		
	Port* src = get_port (s);
	Port* dst = get_port (d);

	if (src && dst) {

		/* both ports are known to us, so do the internal connect stuff */

		if ((ret = src->connect (*dst)) == 0) {
			ret = dst->connect (*src);
		}

	} else if (src || dst) {

		/* one port is known to us, try to connect it to something external */

		PortConnectableByName* pcn;
		string other;

		if (src) {
			pcn = dynamic_cast<PortConnectableByName*>(src);
			other = d;
		} else {
			pcn = dynamic_cast<PortConnectableByName*>(dst);
			other = s;
		}

		if (pcn) {
			ret = pcn->connect (other);
		} else {
			ret = -1;
		}

	} else {

		/* neither port is known to us, and this API isn't intended for use as a general patch bay */

		ret = -1;
		
	}
	
	if (ret > 0) {
		error << string_compose(_("AudioEngine: connection already exists: %1 (%2) to %3 (%4)"), 
					source, s, destination, d) 
		      << endmsg;
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

	Port* src = get_port (s);
	Port* dst = get_port (d);

	if (src && dst) {

		/* both ports are known to us, so do the internal connect stuff */
		
		if ((ret = src->disconnect (*dst)) == 0) {
			ret = dst->disconnect (*src);
		}

	} else if (src || dst) {

		/* one port is known to us, try to connect it to something external */


		PortConnectableByName* pcn;
		string other;

		if (src) {
			pcn = dynamic_cast<PortConnectableByName*>(src);
			other = d;
		} else {
			pcn = dynamic_cast<PortConnectableByName*>(dst);
			other = s;
		}

		if (pcn) {
			ret = pcn->disconnect (other);
		} else {
			ret = -1;
		}

	} else {

		/* neither port is known to us, and this API isn't intended for use as a general patch bay */
		
		ret = -1;
		
	}
	
	return ret;
}

int
AudioEngine::disconnect (Port& port)
{
	if (!_running) {
		if (!_has_run) {
			fatal << _("disconnect called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return -1;
		}
	}

	return port.disconnect_all ();
}

ARDOUR::nframes_t
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

ARDOUR::nframes_t
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

/** Get a port by name.
 * Note this can return NULL, it will NOT create a port if it is not found (any more).
 */
Port *
AudioEngine::get_port_by_name (const string& portname, bool keep) const
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
	
	boost::shared_ptr<Ports> pr = ports.reader();
	
	for (Ports::iterator i = pr->begin(); i != pr->end(); ++i) {
		if (portname == (*i)->name()) {
			return (*i);
		}
	}

	return 0;
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
	ae->_jack = 0;

	if (was_running) {
		ae->Halted(); /* EMIT SIGNAL */
	}
}

uint32_t
AudioEngine::n_physical_outputs () const
{
	const char ** ports;
	uint32_t i = 0;

	if (!_jack) {
		return 0;
	}

	if ((ports = jack_get_ports (_jack, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == 0) {
		return 0;
	}

	if (ports) {
		for (i = 0; ports[i]; ++i);
		free (ports);
	}
	return i;
}

uint32_t
AudioEngine::n_physical_inputs () const
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return 0;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, NULL, JackPortIsPhysical|JackPortIsOutput)) == 0) {
		return 0;
	}

	if (ports) {
		for (i = 0; ports[i]; ++i);
		free (ports);
	}
	return i;
}

void
AudioEngine::get_physical_inputs (vector<string>& ins)
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, NULL, JackPortIsPhysical|JackPortIsOutput)) == 0) {
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
AudioEngine::get_physical_outputs (vector<string>& outs)
{
	const char ** ports;
	uint32_t i = 0;
	
	if (!_jack) {
		return;
	}
	
	if ((ports = jack_get_ports (_jack, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == 0) {
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
AudioEngine::get_nth_physical (DataType type, uint32_t n, int flag)
{
	const char ** ports;
	uint32_t i;
	string ret;

	assert(type != DataType::NIL);

	if (!_running || !_jack) {
		if (!_has_run) {
			fatal << _("get_nth_physical called before engine was started") << endmsg;
			/*NOTREACHED*/
		} else {
			return "";
		}
	}

	ports = jack_get_ports (_jack, NULL, type.to_jack_type(), JackPortIsPhysical|flag);
	
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

ARDOUR::nframes_t
AudioEngine::get_port_total_latency (const Port& port)
{
	return port.total_latency ();
}

void
AudioEngine::update_total_latency (const Port& port)
{
	if (!_jack) {
		fatal << _("update_total_latency() called with no JACK client connection") << endmsg;
		/*NOTREACHED*/
	}

	if (!_running) {
		if (!_has_run) {
			fatal << _("update_total_latency() called before engine was started") << endmsg;
			/*NOTREACHED*/
		} 
	}

	port.recompute_total_latency ();
}

void
AudioEngine::transport_stop ()
{
	// cerr << "tell JACK to stop\n";
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

		if (onoff) {
			_freewheel_thread_registered = false;
		}

		return jack_set_freewheel (_jack, onoff);

	} else {
		return -1;
	}
}

void
AudioEngine::remove_all_ports ()
{
	/* process lock MUST be held */

	{
		RCUWriter<Ports> writer (ports);
		boost::shared_ptr<Ports> ps = writer.get_copy ();
		ps->clear ();
	}
}

void
AudioEngine::remove_connections_for (Port& port)
{
	for (PortConnections::iterator i = port_connections.begin(); i != port_connections.end(); ) {
		PortConnections::iterator tmp;
		
		tmp = i;
		++tmp;
		
		if ((*i).first == port.name()) {
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

		if (status & JackServerFailed) {
			error << _("Unable to connect to JACK server") << endmsg;
		}
		
		// error message is not useful here
		return -1;
	}

	if (status & JackServerStarted) {
		info << _("JACK server started") << endmsg;
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

	if ((_jack = jack_client_new (client_name.c_str())) == NULL) {
		return -1;
	}

	return 0;
}

#endif /* HAVE_JACK_CLIENT_OPEN */

int 
AudioEngine::disconnect_from_jack ()
{
	if (_jack == 0) {
		return 0;
	}

	jack_client_close (_jack);

	_buffer_size = 0;
	_frame_rate = 0;

	if (_running) {
		stop_metering_thread ();
		_running = false;
		Stopped(); /* EMIT SIGNAL */
	}

	_jack = 0;
	return 0;
}

int
AudioEngine::reconnect_to_jack ()
{
	if (_jack) {
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
		if ((*i)->reestablish ()) {
			break;
		} 
	}

	if (i != p->end()) {
		/* failed */
		remove_all_ports ();
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
	
	for (i = p->begin(); i != p->end(); ++i) {
		(*i)->reconnect ();
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
	jack_recompute_total_latencies (_jack);
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
