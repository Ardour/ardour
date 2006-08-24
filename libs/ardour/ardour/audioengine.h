/*
    Copyright (C) 2002-2004 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_audioengine_h__
#define __ardour_audioengine_h__

#include <list>
#include <set>
#include <cmath>
#include <exception>
#include <string>

#include <sigc++/signal.h>

#include <glibmm/thread.h>

#include <pbd/rcu.h>

#include <ardour/ardour.h>
#include <jack/jack.h>
#include <jack/transport.h>
#include <ardour/types.h>
#include <ardour/data_type.h>

namespace ARDOUR {

class Session;
class Port;

class AudioEngine : public sigc::trackable
{
   public:
	AudioEngine (std::string client_name);
	virtual ~AudioEngine ();
	
	jack_client_t* jack() const { return _jack; }
	bool connected() const { return _jack != 0; }

	std::string client_name() const { return jack_client_name; }

	int reconnect_to_jack ();
	int disconnect_from_jack();

	bool will_reconnect_at_halt ();
	void set_reconnect_at_halt (bool);

	int stop ();
	int start ();
	bool running() const { return _running; }

	Glib::Mutex& process_lock() { return _process_lock; }

	jack_nframes_t frame_rate();
	jack_nframes_t frames_per_cycle();

	int usecs_per_cycle () const { return _usecs_per_cycle; }

	jack_nframes_t frames_since_cycle_start () {
		if (!_running || !_jack) return 0;
		return jack_frames_since_cycle_start (_jack);
	}
	jack_nframes_t frame_time () {
		if (!_running || !_jack) return 0;
		return jack_frame_time (_jack);
	}

	jack_nframes_t transport_frame () const {
		if (!_running || !_jack) return 0;
		return jack_get_current_transport_frame (_jack);
	}
	
	int request_buffer_size (jack_nframes_t);
	
	jack_nframes_t set_monitor_check_interval (jack_nframes_t);

	float get_cpu_load() { 
		if (!_running || !_jack) return 0;
		return jack_cpu_load (_jack);
	}

	void set_session (Session *);
	void remove_session ();

	class PortRegistrationFailure : public std::exception {
	  public:
		virtual const char *what() const throw() { return "failed port registration"; }
	};

	class NoBackendAvailable : public std::exception {
	  public:
		virtual const char *what() const throw() { return "could not connect to engine backend"; }
	};

	Port *register_input_port (DataType type, const std::string& portname);
	Port *register_output_port (DataType type, const std::string& portname);
	int   unregister_port (Port &);
	
	int connect (const std::string& source, const std::string& destination);
	int disconnect (const std::string& source, const std::string& destination);
	int disconnect (Port &);
	
	const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags);

	uint32_t n_physical_outputs () const;
	uint32_t n_physical_inputs () const;

	void get_physical_outputs (std::vector<std::string>&);
	void get_physical_inputs (std::vector<std::string>&);

	std::string get_nth_physical_output (DataType type, uint32_t n) {
		return get_nth_physical (type, n, JackPortIsInput);
	}

	std::string get_nth_physical_input (DataType type, uint32_t n) {
		return get_nth_physical (type, n, JackPortIsOutput);
	}

	jack_nframes_t get_port_total_latency (const Port&);
	void update_total_latencies ();

	/** Caller may not delete the object pointed to by the return value
	*/
	Port *get_port_by_name (const std::string& name, bool keep = true);

	enum TransportState {
		TransportStopped = JackTransportStopped,
		TransportRolling = JackTransportRolling,
		TransportLooping = JackTransportLooping,
		TransportStarting = JackTransportStarting
	};

	void transport_start ();
	void transport_stop ();
	void transport_locate (jack_nframes_t);
	TransportState transport_state ();

	int  reset_timebase ();

	/* start/stop freewheeling */

	int freewheel (bool onoff);
	bool freewheeling() const { return _freewheeling; }

	/* this signal is sent for every process() cycle while freewheeling.
	   the regular process() call to session->process() is not made.
	*/

	sigc::signal<int,jack_nframes_t> Freewheel;

	sigc::signal<void> Xrun;

	/* this signal is if JACK notifies us of a graph order event */

	sigc::signal<void> GraphReordered;

	/* this signal is emitted if the sample rate changes */

	sigc::signal<void,jack_nframes_t> SampleRateChanged;

	/* this signal is sent if JACK ever disconnects us */

	sigc::signal<void> Halted;

	/* these two are emitted when the engine itself is
	   started and stopped
	*/

	sigc::signal<void> Running;
	sigc::signal<void> Stopped;

	std::string make_port_name_relative (std::string);
	std::string make_port_name_non_relative (std::string);

  private:
	ARDOUR::Session      *session;
	jack_client_t       *_jack;
	std::string           jack_client_name;
	Glib::Mutex           _process_lock;
	Glib::Mutex           session_remove_lock;
    Glib::Cond            session_removed;
	bool                  session_remove_pending;
	bool                 _running;
	bool                 _has_run;
	jack_nframes_t       _buffer_size;
	jack_nframes_t       _frame_rate;
	jack_nframes_t        monitor_check_interval;
	jack_nframes_t        last_monitor_check;
	jack_nframes_t       _processed_frames;
	bool                 _freewheeling;
	bool                 _freewheel_thread_registered;
	sigc::slot<int,jack_nframes_t>  freewheel_action;
	bool                  reconnect_on_halt;
	int                  _usecs_per_cycle;

	typedef std::set<Port*> Ports;
	SerializedRCUManager<Ports> ports;

	int    process_callback (jack_nframes_t nframes);
	void   remove_all_ports ();

	typedef std::pair<std::string,std::string> PortConnection;
	typedef std::list<PortConnection> PortConnections;

	PortConnections port_connections;
	void   remove_connections_for (Port&);

	std::string get_nth_physical (DataType type, uint32_t n, int flags);

	static int  _xrun_callback (void *arg);
	static int  _graph_order_callback (void *arg);
	static int  _process_callback (jack_nframes_t nframes, void *arg);
	static int  _sample_rate_callback (jack_nframes_t nframes, void *arg);
	static int  _bufsize_callback (jack_nframes_t nframes, void *arg);
	static void _jack_timebase_callback (jack_transport_state_t, jack_nframes_t, jack_position_t*, int, void*);
	static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
	static void _freewheel_callback (int , void *arg);

	void jack_timebase_callback (jack_transport_state_t, jack_nframes_t, jack_position_t*, int);
	int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
	int  jack_bufsize_callback (jack_nframes_t);
	int  jack_sample_rate_callback (jack_nframes_t);

	static void halted (void *);

	int connect_to_jack (std::string client_name);

	void meter_thread ();
	void start_metering_thread ();
    Glib::Thread*    m_meter_thread;
    mutable gint     m_meter_exit;
};

} // namespace ARDOUR

#endif /* __ardour_audioengine_h__ */
