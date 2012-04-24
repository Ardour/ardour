/*
    Copyright (C) 1998 Paul Barton-Davis
    
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

#ifndef  __midi_parse_h__
#define  __midi_parse_h__

#include <string>
#include <iostream>

#include "pbd/signals.h"

#include "midi++/types.h"

namespace MIDI {

class Port;
class Parser;

typedef PBD::Signal1<void,Parser&>                   ZeroByteSignal;
typedef PBD::Signal2<void,Parser&,framecnt_t>        TimestampedSignal;
typedef PBD::Signal2<void,Parser&, byte>             OneByteSignal;
typedef PBD::Signal2<void,Parser &, EventTwoBytes *> TwoByteSignal;
typedef PBD::Signal2<void,Parser &, pitchbend_t>     PitchBendSignal;
typedef PBD::Signal3<void,Parser &, byte *, size_t>  Signal;

class Parser {
 public:
	Parser (Port &p);
	~Parser ();

	/* sets the time that will be reported for any MTC or MIDI Clock
	   message the next time ::scanner() parses such a message. It should
	   therefore be set before every byte passed into ::scanner().
	*/
	
	framecnt_t get_timestamp() const { return _timestamp; }
	void set_timestamp (const framecnt_t timestamp) { _timestamp = timestamp; } 

	/* signals that anyone can connect to */
	
	OneByteSignal         bank_change;
	TwoByteSignal         note_on;
	TwoByteSignal         note_off;
	TwoByteSignal         poly_pressure;
	OneByteSignal         pressure;
	OneByteSignal         program_change;
	PitchBendSignal       pitchbend;
	TwoByteSignal         controller;

	OneByteSignal         channel_bank_change[16];
	TwoByteSignal         channel_note_on[16];
	TwoByteSignal         channel_note_off[16];
	TwoByteSignal         channel_poly_pressure[16];
	OneByteSignal         channel_pressure[16];
	OneByteSignal         channel_program_change[16];
	PitchBendSignal       channel_pitchbend[16];
	TwoByteSignal         channel_controller[16];
	ZeroByteSignal        channel_active_preparse[16];
	ZeroByteSignal        channel_active_postparse[16];

	OneByteSignal         mtc_quarter_frame; /* see below for more useful signals */
	Signal                mtc;
	Signal                raw_preparse;
	Signal                raw_postparse;
	Signal                any;
	Signal                sysex;
	Signal                mmc;
	Signal                position;
	Signal                song;

	ZeroByteSignal        all_notes_off;
	ZeroByteSignal        tune;
	ZeroByteSignal        active_sense;
	ZeroByteSignal        reset;
	ZeroByteSignal        eox;

	TimestampedSignal     timing;
	TimestampedSignal     start;
	TimestampedSignal     stop;
	TimestampedSignal     contineu;  /* note spelling */

	/* This should really be protected, but then derivatives of Port
	   can't access it.
	*/

	void scanner (byte c);

	size_t *message_counts() { return message_counter; }
	const char *midi_event_type_name (MIDI::eventType);
	void trace (bool onoff, std::ostream *o, const std::string &prefix = "");
	bool tracing() { return trace_stream != 0; }
	Port &port() { return _port; }

	void set_offline (bool);
	bool offline() const { return _offline; }
	PBD::Signal0<void> OfflineStatusChanged;

	PBD::Signal2<int,byte *, size_t> edit;

	void set_mmc_forwarding (bool yn) {
		_mmc_forward = yn;
	}

	/* MTC */

	MTC_FPS mtc_fps() const { return _mtc_fps; }
	MTC_Status  mtc_running() const { return _mtc_running; }
	const byte *mtc_current() const { return _mtc_time; }
	bool        mtc_locked() const  { return _mtc_locked; }
	
	PBD::Signal3<void, Parser &, int, framecnt_t>      mtc_qtr;
	PBD::Signal3<void, const byte *, bool, framecnt_t> mtc_time;
	PBD::Signal1<void, MTC_Status>                     mtc_status;
	PBD::Signal0<bool>                                 mtc_skipped;

	void set_mtc_forwarding (bool yn) {
		_mtc_forward = yn;
	}

	void reset_mtc_state ();
	
  private:
	Port&_port;
	/* tracing */
	
	std::ostream *trace_stream;
	std::string trace_prefix;
	void trace_event (Parser &p, byte *msg, size_t len);
	PBD::ScopedConnection trace_connection;

	size_t message_counter[256];

	enum ParseState { 
		NEEDSTATUS,
		NEEDONEBYTE,
		NEEDTWOBYTES,
		VARIABLELENGTH
	};
	ParseState state;
	unsigned char *msgbuf;
	int msglen;
	int msgindex;
	MIDI::eventType msgtype;
	channel_t channel;
	bool _offline;
	bool runnable;
	bool was_runnable;
	bool _mmc_forward;
	bool _mtc_forward;
	int   expected_mtc_quarter_frame_code;
	byte _mtc_time[5];
	byte _qtr_mtc_time[5];
	unsigned long consecutive_qtr_frame_cnt;
	MTC_FPS _mtc_fps;
	MTC_Status _mtc_running;
	bool       _mtc_locked;
	byte last_qtr_frame;
	
	framecnt_t _timestamp;

	ParseState pre_variable_state;
	MIDI::eventType pre_variable_msgtype;
	byte last_status_byte;

	void channel_msg (byte);
	void realtime_msg (byte);
	void system_msg (byte);
	void signal (byte *msg, size_t msglen);
	bool possible_mmc (byte *msg, size_t msglen);
	bool possible_mtc (byte *msg, size_t msglen);
	void process_mtc_quarter_frame (byte *msg);
};

} // namespace MIDI

#endif   // __midi_parse_h__

