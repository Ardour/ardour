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

class Slave {
  public:
	Slave() { }
	virtual ~Slave() {}

	virtual bool speed_and_position (float&, nframes_t&) = 0;
	virtual bool locked() const = 0;
	virtual bool ok() const = 0;
	virtual bool starting() const { return false; }
	virtual nframes_t resolution() const = 0;
	virtual bool requires_seekahead () const = 0;
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
	bool requires_seekahead () const { return true; }

  private:
	Session&    session;
	MIDI::Port* port;
	std::vector<sigc::connection> connections;

	int         ppqn;
	double      one_ppqn_in_frames;

	SafeTime    current;
	nframes_t   midi_clock_frame;               /* current time */
	nframes_t   last_inbound_frame;             /* when we got it; audio clocked */

	float       midi_clock_speed;
	nframes_t   first_midi_clock_frame;
	nframes_t   first_midi_clock_time;

	// keep four beats of history ( 4 * 24 ppqn )
	static const int32_t accumulator_size = 96;
	float   accumulator[accumulator_size];
	int32_t accumulator_index;
	bool    have_first_accumulated_speed;
	float   average;

	void reset ();
	void start (MIDI::Parser& parser, nframes_t timestamp);
	void stop (MIDI::Parser& parser, nframes_t timestamp);
	void update_midi_clock (MIDI::Parser& parser, nframes_t timestamp);
	void read_current (SafeTime *) const;

	bool _started;
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
