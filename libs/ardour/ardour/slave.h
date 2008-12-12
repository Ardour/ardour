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

#include <jack/jack.h>

#include <sigc++/signal.h>
#include <ardour/ardour.h>
#include <midi++/parser.h>
#include <midi++/types.h>

namespace MIDI {
	class Port;
}

namespace ARDOUR {
class Session;

/**
 * @class Slave
 * 
 * @brief The class Slave can be used to sync ardours tempo to an external source
 * like MTC, MIDI Clock, etc.
 * 
 * The name of the class may be a bit misleading: A subclass of Slave actually
 * acts as a master for Ardour, that means Ardour will try to follow the
 * speed and transport position of the implementation of Slave.
 * Therefor it is rather that class, that makes Ardour a slave by connecting it
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
	 * <emph>position</emph> using a delay locked loop (DLL),
	 * starting with the first given transport speed.
	 * If the values of speed and position contradict each other,
	 * ardour will always follow the position and disregard the speed.
	 * Although, a correct speed is important so that ardour
	 * can sync to the master time source quickly.
	 * 
	 * For background information on delay locked loops, 
	 * see http://www.kokkinizita.net/papers/usingdll.pdf
	 * 
	 * The method has the following precondition:
	 * <ul>
	 * 	 <li>
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
	 * 	  <li>
	 *       The first position value on transport start should be 0,
	 *       otherwise ardour will try to locate to the new position 
	 *       rather than move to it
	 *    </li>
	 * 	  <li>
	 *      the references speed and position should be assigned 
	 *      to the Slaves current requested transport speed
	 *      and transport position.
	 *    </li>
	 *   <li>
	 *     Slave::resolution() should be greater than the maximum distance of 
	 *     ardours transport position to the slaves requested transport position.
	 *   </li>
	 * 	 <li>Slave::locked() should return true, otherwise Session::no_roll will be called</li>
	 * 	 <li>Slave::starting() should be false, otherwise the transport will not move until it becomes true</li>	 *   
	 * </ul>
	 * 
	 * @param speed - The transport speed requested
	 * @param position - The transport position requested
	 */
	virtual bool speed_and_position (float& speed, nframes_t& position) = 0;
	
	/**
	 * reports to ardour whether the Slave is currently synced to its external 
	 * time source.
	 * 
	 * @return - when returning false, the transport will stop rolling
	 */
	virtual bool locked() const = 0;
	
	/**
	 * reports to ardour whether the slave is in a sane state
	 * 
	 * @return - when returning false, the transport will be stopped and the slave 
	 * disconnected from ardour.
	 */
	virtual bool ok() const = 0;
	
	/**
	 * reports to ardour whether the slave is in the process of starting
	 * to roll
	 * 
	 * @return - when returning false, transport will not move until this method returns true
	 */
	virtual bool starting() const { return false; }
	
	/**
	 * @return - the timing resolution of the Slave - If the distance of ardours transport
	 * to the slave becomes greater than the resolution, sound will stop
	 */
	virtual nframes_t resolution() const = 0;
	
	/**
	 * @return - when returning true, ardour will wait for one second before transport
	 * starts rolling
	 */
	virtual bool requires_seekahead () const = 0;
	
	/**
	 * @return - when returning true, ardour will use transport speed 1.0 no matter what 
	 *           the slave returns
	 */
	virtual bool is_always_synced() const { return false; }
};

struct SafeTime {
    int guard1;
    nframes_t   position;
    nframes_t   timestamp;
    int guard2;

    SafeTime() {
	    guard1 = 0;
	    guard2 = 0;
	    timestamp = 0;
    }
};

class MTC_Slave : public Slave, public sigc::trackable {
  public:
	MTC_Slave (Session&, MIDI::Port&);
	~MTC_Slave ();

	void rebind (MIDI::Port&);
	bool speed_and_position (float&, nframes_t&);

	bool locked() const;
	bool ok() const;
	void handle_locate (const MIDI::byte*);

	nframes_t resolution() const;
	bool requires_seekahead () const { return true; }

  private:
	Session&    session;
	MIDI::Port* port;
	std::vector<sigc::connection> connections;
	bool        can_notify_on_unknown_rate;

	SafeTime    current;
	nframes_t   mtc_frame;               /* current time */
	nframes_t   last_inbound_frame;      /* when we got it; audio clocked */
	MIDI::byte  last_mtc_fps_byte;

	float       mtc_speed;
	nframes_t   first_mtc_frame;
	nframes_t   first_mtc_time;

	static const int32_t accumulator_size = 128;
	float   accumulator[accumulator_size];
	int32_t accumulator_index;
	bool    have_first_accumulated_speed;

	void reset ();
	void update_mtc_qtr (MIDI::Parser&);
	void update_mtc_time (const MIDI::byte *, bool);
	void update_mtc_status (MIDI::Parser::MTC_Status);
	void read_current (SafeTime *) const;
};

class MIDIClock_Slave : public Slave, public sigc::trackable {
  public:
	MIDIClock_Slave (Session&, MIDI::Port&, int ppqn = 24);
	~MIDIClock_Slave ();

	void rebind (MIDI::Port&);
	bool speed_and_position (float&, nframes_t&);

	bool locked() const;
	bool ok() const;
	bool starting() const { return false; }

	nframes_t resolution() const;
	bool requires_seekahead () const { return false; }

  private:
	Session&    session;
	MIDI::Port* port;
	std::vector<sigc::connection> connections;

	/// pulses per quarter note for one MIDI clock frame (default 24)
	int         ppqn;
	
	/// the duration of one ppqn in frame time
	double      one_ppqn_in_frames;

	/// the time stamp and transport position of the last inbound MIDI clock message
	SafeTime    current;
	/// since current.position is integral, we need to keep track of decimal places
	/// to be precise
	double      current_position;
	
	/// The duration of the current MIDI clock frame in frames
	nframes_t   current_midi_clock_frame_duration;
	/// the timestamp of the last inbound MIDI clock message
	nframes_t   last_inbound_frame;             

	/// how many MIDI clock frames to average over
	static const int32_t accumulator_size = 4;
	double  accumulator[accumulator_size];
	int32_t accumulator_index;
	
	/// the running average of current_midi_clock_frame_duration
	double  average_midi_clock_frame_duration;

	void reset ();
	void start (MIDI::Parser& parser, nframes_t timestamp);
	void stop (MIDI::Parser& parser, nframes_t timestamp);
	void update_midi_clock (MIDI::Parser& parser, nframes_t timestamp);
	void read_current (SafeTime *) const;

	/// whether transport should be rolling
	bool _started;
	
	/// is true if the MIDI Start message has just been received until
	/// the first call of speed_and_position(...)
	bool _starting;
};

class ADAT_Slave : public Slave
{
  public:
	ADAT_Slave () {}
	~ADAT_Slave () {}

	bool speed_and_position (float& speed, nframes_t& pos) {
		speed = 0;
		pos = 0;
		return false;
	}

	bool locked() const { return false; }
	bool ok() const { return false; }
	nframes_t resolution() const { return 1; }
	bool requires_seekahead () const { return true; }
};

class JACK_Slave : public Slave
{
  public:
	JACK_Slave (jack_client_t*);
	~JACK_Slave ();

	bool speed_and_position (float& speed, nframes_t& pos);

	bool starting() const { return _starting; }
	bool locked() const;
	bool ok() const;
	nframes_t resolution() const { return 1; }
	bool requires_seekahead () const { return false; }
	void reset_client (jack_client_t* jack);
	bool is_always_synced() const { return true; }

  private:
	jack_client_t* jack;
	float speed;
	bool _starting;
};

} /* namespace */

#endif /* __ardour_slave_h__ */
