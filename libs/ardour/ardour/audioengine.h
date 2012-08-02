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

*/

#ifndef __ardour_audioengine_h__
#define __ardour_audioengine_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <list>
#include <set>
#include <cmath>
#include <exception>
#include <string>

#include <glibmm/threads.h>

#include "pbd/rcu.h"
#include "pbd/signals.h"
#include "pbd/stacktrace.h"

#include <jack/weakjack.h>
#include <jack/jack.h>
#include <jack/transport.h>
#include <jack/thread.h>

#include "ardour/ardour.h"

#include "ardour/data_type.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"
#include "ardour/chan_count.h"

#ifdef HAVE_JACK_SESSION
#include <jack/session.h>
#endif

namespace ARDOUR {

class InternalPort;
class MidiPort;
class Port;
class Session;
class ProcessThread;

class AudioEngine : public SessionHandlePtr
{
public:
	typedef std::map<std::string,boost::shared_ptr<Port> > Ports;

	AudioEngine (std::string client_name, std::string session_uuid);
	virtual ~AudioEngine ();

	jack_client_t* jack() const;
	bool connected() const { return _jack != 0; }

	bool is_realtime () const;

	ProcessThread* main_thread() const { return _main_thread; }

	std::string client_name() const { return jack_client_name; }

	int reconnect_to_jack ();
	int disconnect_from_jack();

	int stop (bool forever = false);
	int start ();
	bool running() const { return _running; }

	Glib::Threads::Mutex& process_lock() { return _process_lock; }

	framecnt_t frame_rate () const;
	pframes_t frames_per_cycle () const;

	size_t raw_buffer_size(DataType t);

	int usecs_per_cycle () const { return _usecs_per_cycle; }

	bool get_sync_offset (pframes_t & offset) const;

	pframes_t frames_since_cycle_start () {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) {
			return 0;
		}
		return jack_frames_since_cycle_start (_priv_jack);
	}

	pframes_t frame_time () {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) {
			return 0;
		}
		return jack_frame_time (_priv_jack);
	}

	pframes_t frame_time_at_cycle_start () {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) {
			return 0;
		}
		return jack_last_frame_time (_priv_jack);
	}

	pframes_t transport_frame () const {
  	        const jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) {
			return 0;
		}
		return jack_get_current_transport_frame (_priv_jack);
	}

	int request_buffer_size (pframes_t);

	framecnt_t processed_frames() const { return _processed_frames; }

	float get_cpu_load() {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) {
			return 0;
		}
		return jack_cpu_load (_priv_jack);
	}

	void set_session (Session *);
	void remove_session (); // not a replacement for SessionHandle::session_going_away()

	class PortRegistrationFailure : public std::exception {
	public:
		PortRegistrationFailure (std::string const & why = "")
			: reason (why) {}

		~PortRegistrationFailure () throw () {}

		virtual const char *what() const throw () { return reason.c_str(); }

	private:
		std::string reason;
	};

	class NoBackendAvailable : public std::exception {
	public:
		virtual const char *what() const throw() { return "could not connect to engine backend"; }
	};

	boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname);
	boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname);
	int unregister_port (boost::shared_ptr<Port>);

	bool port_is_physical (const std::string&) const;
	void request_jack_monitors_input (const std::string&, bool) const;

	void split_cycle (pframes_t offset);

	int connect (const std::string& source, const std::string& destination);
	int disconnect (const std::string& source, const std::string& destination);
	int disconnect (boost::shared_ptr<Port>);

	const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags);

	bool can_request_hardware_monitoring ();

	ChanCount n_physical_outputs () const;
	ChanCount n_physical_inputs () const;

	void get_physical_outputs (DataType type, std::vector<std::string>&);
	void get_physical_inputs (DataType type, std::vector<std::string>&);

	boost::shared_ptr<Port> get_port_by_name (const std::string &);
	void port_renamed (const std::string&, const std::string&);

	enum TransportState {
		TransportStopped = JackTransportStopped,
		TransportRolling = JackTransportRolling,
		TransportLooping = JackTransportLooping,
		TransportStarting = JackTransportStarting
	};

	void transport_start ();
	void transport_stop ();
	void transport_locate (framepos_t);
	TransportState transport_state ();

	int  reset_timebase ();

        void update_latencies ();

	/* start/stop freewheeling */

	int freewheel (bool onoff);
	bool freewheeling() const { return _freewheeling; }

	/* this signal is sent for every process() cycle while freewheeling.
_	   the regular process() call to session->process() is not made.
	*/

	PBD::Signal1<int, pframes_t> Freewheel;

	PBD::Signal0<void> Xrun;

	/* this signal is if JACK notifies us of a graph order event */

	PBD::Signal0<void> GraphReordered;

#ifdef HAVE_JACK_SESSION
	PBD::Signal1<void,jack_session_event_t *> JackSessionEvent;
#endif


	/* this signal is emitted if the sample rate changes */

	PBD::Signal1<void, framecnt_t> SampleRateChanged;

	/* this signal is sent if JACK ever disconnects us */

	PBD::Signal1<void,const char*> Halted;

	/* these two are emitted when the engine itself is
	   started and stopped
	*/

	PBD::Signal0<void> Running;
	PBD::Signal0<void> Stopped;

	/** Emitted if a JACK port is registered or unregistered */
	PBD::Signal0<void> PortRegisteredOrUnregistered;

	/** Emitted if a JACK port is connected or disconnected.
	 *  The Port parameters are the ports being connected / disconnected, or 0 if they are not known to Ardour.
	 *  The std::string parameters are the (long) port names.
	 *  The bool parameter is true if ports were connected, or false for disconnected.
	 */
	PBD::Signal5<void, boost::weak_ptr<Port>, std::string, boost::weak_ptr<Port>, std::string, bool> PortConnectedOrDisconnected;

	std::string make_port_name_relative (std::string) const;
	std::string make_port_name_non_relative (std::string) const;
	bool port_is_mine (const std::string&) const;

	static AudioEngine* instance() { return _instance; }
	static void destroy();
	void died ();

	int create_process_thread (boost::function<void()>, pthread_t*, size_t stacksize);

private:
	static AudioEngine*       _instance;

	jack_client_t* volatile   _jack; /* could be reset to null by SIGPIPE or another thread */
	std::string                jack_client_name;
	Glib::Threads::Mutex      _process_lock;
        Glib::Threads::Cond        session_removed;
	bool                       session_remove_pending;
        frameoffset_t              session_removal_countdown;
        gain_t                     session_removal_gain;
        gain_t                     session_removal_gain_step;
	bool                      _running;
	bool                      _has_run;
	mutable framecnt_t        _buffer_size;
	std::map<DataType,size_t> _raw_buffer_sizes;
	mutable framecnt_t        _frame_rate;
	/// number of frames between each check for changes in monitor input
	framecnt_t                 monitor_check_interval;
	/// time of the last monitor check in frames
	framecnt_t                 last_monitor_check;
	/// the number of frames processed since start() was called
	framecnt_t                _processed_frames;
	bool                      _freewheeling;
	bool                      _pre_freewheel_mmc_enabled;
	int                       _usecs_per_cycle;
	bool                       port_remove_in_progress;
        Glib::Threads::Thread*     m_meter_thread;
	ProcessThread*            _main_thread;

	SerializedRCUManager<Ports> ports;

	boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input);

	int    process_callback (pframes_t nframes);
	void*  process_thread ();
	void   remove_all_ports ();

	ChanCount n_physical (unsigned long) const;
	void get_physical (DataType, unsigned long, std::vector<std::string> &);

	void port_registration_failure (const std::string& portname);

	static int  _xrun_callback (void *arg);
#ifdef HAVE_JACK_SESSION
	static void _session_callback (jack_session_event_t *event, void *arg);
#endif
	static int  _graph_order_callback (void *arg);
	static void* _process_thread (void *arg);
	static int  _sample_rate_callback (pframes_t nframes, void *arg);
	static int  _bufsize_callback (pframes_t nframes, void *arg);
	static void _jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int, void*);
	static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
	static void _freewheel_callback (int , void *arg);
	static void _registration_callback (jack_port_id_t, int, void *);
	static void _connect_callback (jack_port_id_t, jack_port_id_t, int, void *);

	void jack_timebase_callback (jack_transport_state_t, pframes_t, jack_position_t*, int);
	int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
	int  jack_bufsize_callback (pframes_t);
	int  jack_sample_rate_callback (pframes_t);
	void freewheel_callback (int);

	void set_jack_callbacks ();

	static void _latency_callback (jack_latency_callback_mode_t, void*);
	void jack_latency_callback (jack_latency_callback_mode_t);

	int connect_to_jack (std::string client_name, std::string session_uuid);

	static void halted (void *);
	static void halted_info (jack_status_t,const char*,void *);

	void meter_thread ();
	void start_metering_thread ();
	void stop_metering_thread ();

	static gint      m_meter_exit;

	struct ThreadData {
		AudioEngine* engine;
		boost::function<void()> f;
		size_t stacksize;

		ThreadData (AudioEngine* ae, boost::function<void()> fp, size_t stacksz)
		: engine (ae) , f (fp) , stacksize (stacksz) {}
	};

	static void* _start_process_thread (void*);
};

} // namespace ARDOUR

#endif /* __ardour_audioengine_h__ */
