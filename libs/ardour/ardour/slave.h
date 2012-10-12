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

#ifndef __ardour_slave_h__
#define __ardour_slave_h__

#include <vector>

#include <glibmm/threads.h>

#include <jack/jack.h>

#include "pbd/signals.h"

#include "ardour/types.h"
#include "midi++/parser.h"
#include "midi++/types.h"

namespace MIDI {
	class Port;
}

namespace ARDOUR {

class TempoMap;
class Session;

/**
 * @class Slave
 *
 * @brief The Slave interface can be used to sync ARDOURs tempo to an external source
 * like MTC, MIDI Clock, etc.
 *
 * The name of the interface may be a bit misleading: A subclass of Slave actually
 * acts as a time master for ARDOUR, that means ARDOUR will try to follow the
 * speed and transport position of the implementation of Slave.
 * Therefore it is rather that class, that makes ARDOUR a slave by connecting it
 * to its external time master.
 */
class Slave {
  public:
	Slave() { }
	virtual ~Slave() {}

	/**
	 * This is the most important function to implement:
	 * Each process cycle, Session::follow_slave will call this method.
	 *  and after the method call they should
	 *
	 * Session::follow_slave will then try to follow the given
	 * <em>position</em> using a delay locked loop (DLL),
	 * starting with the first given transport speed.
	 * If the values of speed and position contradict each other,
	 * ARDOUR will always follow the position and disregard the speed.
	 * Although, a correct speed is important so that ARDOUR
	 * can sync to the master time source quickly.
	 *
	 * For background information on delay locked loops,
	 * see http://www.kokkinizita.net/papers/usingdll.pdf
	 *
	 * The method has the following precondition:
	 * <ul>
	 *   <li>
	 *       Slave::ok() should return true, otherwise playback will stop
	 *       immediately and the method will not be called
	 *   </li>
	 *   <li>
	 *     when the references speed and position are passed into the Slave
	 *     they are uninitialized
	 *   </li>
	 * </ul>
	 *
	 * After the method call the following postconditions should be met:
	 * <ul>
	 *    <li>
	 *       The first position value on transport start should be 0,
	 *       otherwise ARDOUR will try to locate to the new position
	 *       rather than move to it
	 *    </li>
	 *    <li>
	 *      the references speed and position should be assigned
	 *      to the Slaves current requested transport speed
	 *      and transport position.
	 *    </li>
	 *   <li>
	 *     Slave::resolution() should be greater than the maximum distance of
	 *     ARDOURs transport position to the slaves requested transport position.
	 *   </li>
	 *   <li>Slave::locked() should return true, otherwise Session::no_roll will be called</li>
	 *   <li>Slave::starting() should be false, otherwise the transport will not move until it becomes true</li>	 *
	 * </ul>
	 *
	 * @param speed - The transport speed requested
	 * @param position - The transport position requested
	 * @return - The return value is currently ignored (see Session::follow_slave)
	 */
	virtual bool speed_and_position (double& speed, framepos_t& position) = 0;

	/**
	 * reports to ARDOUR whether the Slave is currently synced to its external
	 * time source.
	 *
	 * @return - when returning false, the transport will stop rolling
	 */
	virtual bool locked() const = 0;

	/**
	 * reports to ARDOUR whether the slave is in a sane state
	 *
	 * @return - when returning false, the transport will be stopped and the slave
	 * disconnected from ARDOUR.
	 */
	virtual bool ok() const = 0;

	/**
	 * reports to ARDOUR whether the slave is in the process of starting
	 * to roll
	 *
	 * @return - when returning false, transport will not move until this method returns true
	 */
	virtual bool starting() const { return false; }

	/**
	 * @return - the timing resolution of the Slave - If the distance of ARDOURs transport
	 * to the slave becomes greater than the resolution, sound will stop
	 */
	virtual framecnt_t resolution() const = 0;

	/**
	 * @return - when returning true, ARDOUR will wait for seekahead_distance() before transport
	 * starts rolling
	 */
	virtual bool requires_seekahead () const = 0;

	/**
	 * @return the number of frames that this slave wants to seek ahead. Relevant
	 * only if requires_seekahead() returns true.
	 */

	virtual framecnt_t seekahead_distance() const { return 0; }

	/**
	 * @return - when returning true, ARDOUR will use transport speed 1.0 no matter what
	 *           the slave returns
	 */
	virtual bool is_always_synced() const { return false; }

	/**
	 * @return - whether ARDOUR should use the slave speed without any adjustments
	 */
	virtual bool give_slave_full_control_over_transport_speed() const { return false; }
};

/// We need this wrapper for testability, it's just too hard to mock up a session class
class ISlaveSessionProxy {
  public:
	virtual ~ISlaveSessionProxy() {}
	virtual TempoMap&  tempo_map()                 const   { return *((TempoMap *) 0); }
	virtual framecnt_t frame_rate()                const   { return 0; }
	virtual framepos_t audible_frame ()            const   { return 0; }
	virtual framepos_t transport_frame ()          const   { return 0; }
	virtual pframes_t  frames_since_cycle_start () const   { return 0; }
	virtual framepos_t frame_time ()               const   { return 0; }

	virtual void request_locate (framepos_t /*frame*/, bool with_roll = false) {
		(void) with_roll;
	}
	virtual void request_transport_speed (double /*speed*/)                   {}
};


/// The Session Proxy for use in real Ardour
class SlaveSessionProxy : public ISlaveSessionProxy {
	Session&    session;

  public:
	SlaveSessionProxy(Session &s) : session(s) {}

	TempoMap&  tempo_map()                 const;
	framecnt_t frame_rate()                const;
	framepos_t audible_frame ()            const;
	framepos_t transport_frame ()          const;
	pframes_t  frames_since_cycle_start () const;
	framepos_t frame_time ()               const;

	void request_locate (framepos_t frame, bool with_roll = false);
	void request_transport_speed (double speed);
};

struct SafeTime {
	volatile int guard1;
	framepos_t   position;
	framepos_t   timestamp;
	double       speed;
	volatile int guard2;

	SafeTime() {
		guard1 = 0;
		position = 0;
		timestamp = 0;
		speed = 0;
		guard2 = 0;
	}
};

class MTC_Slave : public Slave {
  public:
	MTC_Slave (Session&, MIDI::Port&);
	~MTC_Slave ();

	void rebind (MIDI::Port&);
	bool speed_and_position (double&, framepos_t&);

	bool locked() const;
	bool ok() const;
	void handle_locate (const MIDI::byte*);

	framecnt_t resolution () const;
	bool requires_seekahead () const { return true; }
	framecnt_t seekahead_distance() const;
	bool give_slave_full_control_over_transport_speed() const;

  private:
	Session&    session;
	MIDI::Port* port;
	PBD::ScopedConnectionList port_connections;
	bool        can_notify_on_unknown_rate;

	static const int frame_tolerance;

	SafeTime       current;
	framepos_t     mtc_frame;               /* current time */
	framepos_t     last_inbound_frame;      /* when we got it; audio clocked */
	MIDI::byte     last_mtc_fps_byte;
	framepos_t     window_begin;
	framepos_t     window_end;
	framepos_t     first_mtc_timestamp;
	bool           did_reset_tc_format;
	Timecode::TimecodeFormat saved_tc_format;
	Glib::Threads::Mutex    reset_lock;
	uint32_t       reset_pending;
	bool           reset_position;
	int            transport_direction;
	int            busy_guard1;
	int            busy_guard2;

	double         speedup_due_to_tc_mismatch;
	double         quarter_frame_duration;
	Timecode::TimecodeFormat mtc_timecode;
	Timecode::TimecodeFormat a3e_timecode;
	bool           printed_timecode_warning;

	/* DLL - chase MTC */
	double t0; ///< time at the beginning of the MTC quater frame
	double t1; ///< calculated end of the MTC quater frame
	double e2; ///< second order loop error
	double b, c, omega; ///< DLL filter coefficients

	/* DLL - sync engine */
	int    engine_dll_initstate;
	double te0; ///< time at the beginning of the engine process
	double te1; ///< calculated sync time
	double ee2; ///< second order loop error
	double be, ce, oe; ///< DLL filter coefficients

	void reset (bool with_pos);
	void queue_reset (bool with_pos);
	void maybe_reset ();

	void update_mtc_qtr (MIDI::Parser&, int, framepos_t);
	void update_mtc_time (const MIDI::byte *, bool, framepos_t);
	void update_mtc_status (MIDI::MTC_Status);
	void read_current (SafeTime *) const;
	void reset_window (framepos_t);
	bool outside_window (framepos_t) const;
	void init_mtc_dll(framepos_t, double);
	void init_engine_dll (framepos_t, framepos_t);
};

class MIDIClock_Slave : public Slave {
  public:
	MIDIClock_Slave (Session&, MIDI::Port&, int ppqn = 24);

	/// Constructor for unit tests
	MIDIClock_Slave (ISlaveSessionProxy* session_proxy = 0, int ppqn = 24);
	~MIDIClock_Slave ();

	void rebind (MIDI::Port&);
	bool speed_and_position (double&, framepos_t&);

	bool locked() const;
	bool ok() const;
	bool starting() const;

	framecnt_t resolution () const;
	bool requires_seekahead () const { return false; }
	bool give_slave_full_control_over_transport_speed() const { return true; }

	void set_bandwidth (double a_bandwith) { bandwidth = a_bandwith; }

  protected:
	ISlaveSessionProxy* session;
	MIDI::Port* port;
	PBD::ScopedConnectionList port_connections;

	/// pulses per quarter note for one MIDI clock frame (default 24)
	int         ppqn;

	/// the duration of one ppqn in frame time
	double      one_ppqn_in_frames;

	/// the timestamp of the first MIDI clock message
	framepos_t  first_timestamp;

	/// the time stamp and should-be transport position of the last inbound MIDI clock message
	framepos_t  last_timestamp;
	double      should_be_position;

	/// the number of midi clock messages received (zero-based)
	/// since start
	long midi_clock_count;

	//the delay locked loop (DLL), see www.kokkinizita.net/papers/usingdll.pdf

	/// time at the beginning of the MIDI clock frame
	double t0;

	/// calculated end of the MIDI clock frame
	double t1;

	/// loop error = real value - expected value
	double e;

	/// second order loop error
	double e2;

	/// DLL filter bandwidth
	double bandwidth;

	/// DLL filter coefficients
	double b, c, omega;

	void reset ();
	void start (MIDI::Parser& parser, framepos_t timestamp);
	void contineu (MIDI::Parser& parser, framepos_t timestamp);
	void stop (MIDI::Parser& parser, framepos_t timestamp);
	void position (MIDI::Parser& parser, MIDI::byte* message, size_t size);
	// we can't use continue because it is a C++ keyword
	void calculate_one_ppqn_in_frames_at(framepos_t time);
	framepos_t calculate_song_position(uint16_t song_position_in_sixteenth_notes);
	void calculate_filter_coefficients();
	void update_midi_clock (MIDI::Parser& parser, framepos_t timestamp);
	void read_current (SafeTime *) const;
	bool stop_if_no_more_clock_events(framepos_t& pos, framepos_t now);

	/// whether transport should be rolling
	bool _started;

	/// is true if the MIDI Start message has just been received until
	/// the first MIDI Clock Event
	bool _starting;
};

class JACK_Slave : public Slave
{
  public:
	JACK_Slave (jack_client_t*);
	~JACK_Slave ();

	bool speed_and_position (double& speed, framepos_t& pos);

	bool starting() const { return _starting; }
	bool locked() const;
	bool ok() const;
	framecnt_t resolution () const { return 1; }
	bool requires_seekahead () const { return false; }
	void reset_client (jack_client_t* jack);
	bool is_always_synced() const { return true; }

  private:
	jack_client_t* jack;
	double speed;
	bool _starting;
};

} /* namespace */

#endif /* __ardour_slave_h__ */
