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

#include <sigc++/sigc++.h>

#include <midi++/types.h>
#include <midi++/parser.h>

namespace MIDI {

class Port;

class Channel : public sigc::trackable {

  public:
	Channel (byte channel_number, Port &);

	Port &midi_port()               { return port; }
	byte channel()                  { return channel_number; }
	byte program()                  { return program_number; }
	byte bank()                     { return bank_number; }
	byte pressure ()                { return chanpress; }
	byte poly_pressure (byte n)     { return polypress[n]; }

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
		return pitch_bend;
	}

	controller_value_t controller_value (byte n) { 
		return controller_val[n%128];
	}

	controller_value_t *controller_addr (byte n) {
		return &controller_val[n%128];
	}

	void set_controller (byte n, byte val) {
		controller_val[n%128] = val;
	}

	int channel_msg (byte id, byte val1, byte val2);

	int all_notes_off () {
		return channel_msg (MIDI::controller, 123, 0);
	}
	
	int control (byte id, byte value) {
		return channel_msg (MIDI::controller, id, value);
	}
	
	int note_on (byte note, byte velocity) {
		return channel_msg (MIDI::on, note, velocity);
	}
	
	int note_off (byte note, byte velocity) {
		return channel_msg (MIDI::off, note, velocity);
	}
	
	int aftertouch (byte value) {
		return channel_msg (MIDI::chanpress, value, 0);
	}

	int poly_aftertouch (byte note, byte value) {
		return channel_msg (MIDI::polypress, note, value);
	}

	int program_change (byte value) {
		return channel_msg (MIDI::program, value, 0);
	}

	int pitchbend (byte msb, byte lsb) {
		return channel_msg (MIDI::pitchbend, lsb, msb);
	}

  protected:
	friend class Port;
	void connect_input_signals ();
	void connect_output_signals ();

  private:
	Port &port;

	/* Current channel values */

	byte     channel_number;
        byte     bank_number;
	byte     program_number;
	byte     rpn_msb;
	byte     rpn_lsb;
	byte     nrpn_msb;
	byte     nrpn_lsb;
	byte     chanpress;
	byte     polypress[128];
	bool         controller_14bit[128];
	controller_value_t  controller_val[128];
	byte     controller_msb[128];
	byte     controller_lsb[128];
	byte     _last_note_on;
	byte     _last_on_velocity;
	byte     _last_note_off;
	byte     _last_off_velocity;
	pitchbend_t  pitch_bend;
	bool         _omni;
	bool         _poly;
	bool         _mono;
	size_t       _notes_on;

	void reset (bool notes_off = true);
	
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




