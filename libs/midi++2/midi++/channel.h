/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __midichannel_h__
#define __midichannel_h__

#include <queue>

#include "pbd/signals.h"
#include "midi++/parser.h"

namespace MIDI {

class Port;

/** Stateful MIDI channel class.
 *
 * This remembers various useful information about the current 'state' of a
 * MIDI channel (eg current pitch bend value).
 */
class LIBMIDIPP_API Channel : public PBD::ScopedConnectionList {

  public:
	Channel (byte channel_number, Port &);

	Port &midi_port()           { return _port; }
	byte channel()                  { return _channel_number; }
	byte program()                  { return _program_number; }
	byte bank()                     { return _bank_number; }
	byte pressure ()                { return _chanpress; }
	byte poly_pressure (byte n)     { return _polypress[n]; }

	byte last_note_on () { 
		return _last_note_on;
	}
	byte last_on_velocity () { 
		return _last_on_velocity;
	}
	byte last_note_off () { 
		return _last_note_off;
	}
	byte last_off_velocity () { 
		return _last_off_velocity;
	}

	pitchbend_t pitchbend () { 
		return _pitch_bend;
	}

	controller_value_t controller_value (byte n) { 
		return _controller_val[n%128];
	}

	controller_value_t *controller_addr (byte n) {
		return &_controller_val[n%128];
	}

	void set_controller (byte n, byte val) {
		_controller_val[n%128] = val;
	}

	bool channel_msg (byte id, byte val1, byte val2, timestamp_t timestamp);
	bool all_notes_off (timestamp_t timestamp) {
		return channel_msg (MIDI::controller, 123, 0, timestamp);
	}
	
	bool control (byte id, byte value, timestamp_t timestamp) {
		return channel_msg (MIDI::controller, id, value, timestamp);
	}
	
	bool note_on (byte note, byte velocity, timestamp_t timestamp) {
		return channel_msg (MIDI::on, note, velocity, timestamp);
	}
	
	bool note_off (byte note, byte velocity, timestamp_t timestamp) {
		return channel_msg (MIDI::off, note, velocity, timestamp);
	}
	
	bool aftertouch (byte value, timestamp_t timestamp) {
		return channel_msg (MIDI::chanpress, value, 0, timestamp);
	}

	bool poly_aftertouch (byte note, byte value, timestamp_t timestamp) {
		return channel_msg (MIDI::polypress, note, value, timestamp);
	}

	bool program_change (byte value, timestamp_t timestamp) {
		return channel_msg (MIDI::program, value, 0, timestamp);
	}

	bool pitchbend (byte msb, byte lsb, timestamp_t timestamp) {
		return channel_msg (MIDI::pitchbend, lsb, msb, timestamp);
	}

  protected:
	friend class Port;
	void connect_signals ();

  private:
	Port& _port;

	/* Current channel values */
	byte               _channel_number;
	byte               _bank_number;
	byte               _program_number;
	byte               _rpn_msb;
	byte               _rpn_lsb;
	byte               _nrpn_msb;
	byte               _nrpn_lsb;
	byte               _chanpress;
	byte               _polypress[128];
	bool               _controller_14bit[128];
	controller_value_t _controller_val[128];
	byte               _controller_msb[128];
	byte               _controller_lsb[128];
	byte               _last_note_on;
	byte               _last_on_velocity;
	byte               _last_note_off;
	byte               _last_off_velocity;
	pitchbend_t        _pitch_bend;
	bool               _omni;
	bool               _poly;
	bool               _mono;
	size_t             _notes_on;

	void reset (timestamp_t timestamp, framecnt_t nframes, bool notes_off = true);
	
	void process_note_off (Parser &, EventTwoBytes *);
	void process_note_on (Parser &, EventTwoBytes *);
	void process_controller (Parser &, EventTwoBytes *);
	void process_polypress (Parser &, EventTwoBytes *);
	void process_program_change (Parser &, byte);
	void process_chanpress (Parser &, byte);
	void process_pitchbend (Parser &, pitchbend_t);
	void process_reset (Parser &);
};

} // namespace MIDI

#endif // __midichannel_h__




