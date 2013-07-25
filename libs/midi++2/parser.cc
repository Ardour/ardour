/*
    Copyright (C) 1998 Paul Barton-Davis

    This file was inspired by the MIDI parser for KeyKit by 
    Tim Thompson. 
    
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

#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <iterator>

#include "midi++/types.h"
#include "midi++/parser.h"
#include "midi++/port.h"
#include "midi++/mmc.h"
#include "pbd/transmitter.h"

using namespace std;
using namespace MIDI;

const char *
Parser::midi_event_type_name (eventType t)

{
	switch (t) {
	case none:
		return "no midi messages";

	case raw:
		return "raw midi data";

	case MIDI::any:
		return "any midi message";
	  
	case off:
		return "note off";
	  
	case on:
		return "note on";
	  
	case polypress:
		return "aftertouch";
	  
	case MIDI::controller:
		return "controller";
	  
	case program:
		return "program change";
	  
	case chanpress:
		return "channel pressure";
	  
	case MIDI::pitchbend:
		return "pitch bend";
	  
	case MIDI::sysex:
		return "system exclusive";
	  
	case MIDI::song:
		return "song position";
	  
	case MIDI::tune:
		return "tune";
	  
	case MIDI::eox:
		return "end of sysex";
	  
	case MIDI::timing:
		return "timing";
	  
	case MIDI::start:
		return "start";
	  
	case MIDI::stop:
		return "continue";
	  
	case MIDI::contineu:
		return "stop";
	  
	case active:
		return "active sense";
	  
	default:
		return "unknow MIDI event type";
	}
}

Parser::Parser (Port &p) 
	: _port(p)
{
	trace_stream = 0;
	trace_prefix = "";
	memset (message_counter, 0, sizeof (message_counter[0]) * 256);
	msgindex = 0;
	msgtype = none;
	msglen = 256;
	msgbuf = (unsigned char *) malloc (msglen);
	msgbuf[msgindex++] = 0x90;
	_mmc_forward = false;
	reset_mtc_state ();
	_offline = false;

	/* this hack deals with the possibility of our first MIDI
	   bytes being running status messages.
	*/

	channel_msg (0x90);
	state = NEEDSTATUS;

	pre_variable_state = NEEDSTATUS;
	pre_variable_msgtype = none;
}

Parser::~Parser ()

{
	delete msgbuf;
}

void
Parser::trace_event (Parser &, MIDI::byte *msg, size_t len)
{
	eventType type;
	ostream *o;

	if ((o = trace_stream) == NULL) { /* can be asynchronously removed */
		return;
	}
	
	type = (eventType) (msg[0]&0xF0);

	switch (type) {
	case off:
		*o << trace_prefix
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " NoteOff NoteNum "
		   << (int) msg[1]
		   << " Vel "
		   << (int) msg[2]
		   << endmsg;
		break;
		
	case on:
		*o << trace_prefix
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " NoteOn NoteNum "
		   << (int) msg[1]
		   << " Vel "
		   << (int) msg[2]
		   << endmsg;
		break;
	    
	case polypress:
		*o << trace_prefix
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " PolyPressure"
		   << (int) msg[1]
		   << endmsg;
		break;
	    
	case MIDI::controller:
		*o << trace_prefix
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " Controller "
		   << (int) msg[1]
		   << " Value "
		   << (int) msg[2]
		   << endmsg;
		break;
		
	case program:
		*o << trace_prefix 
		   << "Channel "
		   << (msg[0]&0xF)+1
		   <<  " Program Change ProgNum "
		   << (int) msg[1]
		   << endmsg;
		break;
		
	case chanpress:
		*o << trace_prefix 
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " Channel Pressure "
		   << (int) msg[1]
		   << endmsg;
		break;
	    
	case MIDI::pitchbend:
		*o << trace_prefix
		   << "Channel "
		   << (msg[0]&0xF)+1
		   << " Pitch Bend "
		   << ((msg[2]<<7)|msg[1])
		   << endmsg;
		break;
	    
	case MIDI::sysex:
		if (len == 1) {
			switch (msg[0]) {
			case 0xf8:
				*o << trace_prefix
				   << "Clock"
				   << endmsg;
				break;
			case 0xfa:
				*o << trace_prefix
				   << "Start"
				   << endmsg;
				break;
			case 0xfb:
				*o << trace_prefix
				   << "Continue"
				   << endmsg;
				break;
			case 0xfc:
				*o << trace_prefix
				   << "Stop"
				   << endmsg;
				break;
			case 0xfe:
				*o << trace_prefix
				   << "Active Sense"
				   << endmsg;
				break;
			case 0xff:
				*o << trace_prefix
				   << "System Reset"
				   << endmsg;
				break;
			default:
				*o << trace_prefix
				   << "System Exclusive (1 byte : " << hex << (int) *msg << dec << ')'
				   << endmsg;		
				break;
			} 
		} else {
			*o << trace_prefix
			   << "System Exclusive (" << len << ") = [ " << hex;
			for (unsigned int i = 0; i < len; ++i) {
				*o << (int) msgbuf[i] << ' ';
			}
			*o << dec << ']' << endmsg;
			
		}
		break;
	    
	case MIDI::song:
		*o << trace_prefix << "Song" << endmsg;
		break;
	    
	case MIDI::tune:
		*o << trace_prefix << "Tune" << endmsg;
		break;
	    
	case MIDI::eox:
		*o << trace_prefix << "End-of-System Exclusive" << endmsg;
		break;
	    
	case MIDI::timing:
		*o << trace_prefix << "Timing" << endmsg;
		break;
	    
	case MIDI::start:
		*o << trace_prefix << "Start" << endmsg;
		break;
	    
	case MIDI::stop:
		*o << trace_prefix << "Stop" << endmsg;
		break;
	    
	case MIDI::contineu:
		*o << trace_prefix << "Continue" << endmsg;
		break;
	    
	case active:
		*o << trace_prefix << "Active Sense" << endmsg;
		break;
	    
	default:
		*o << trace_prefix << "Unrecognized MIDI message" << endmsg;
		break;
	}
}

void
Parser::trace (bool onoff, ostream *o, const string &prefix)
{
	trace_connection.disconnect ();

	if (onoff) {
		trace_stream = o;
		trace_prefix = prefix;
		any.connect_same_thread (trace_connection, boost::bind (&Parser::trace_event, this, _1, _2, _3));
	} else {
		trace_prefix = "";
		trace_stream = 0;
	}
}

void
Parser::scanner (unsigned char inbyte)
{
	bool statusbit;
        boost::optional<int> edit_result;

	// cerr << "parse: " << hex << (int) inbyte << dec << " state = " << state << " msgindex = " << msgindex << " runnable = " << runnable << endl;
	
	/* Check active sensing early, so it doesn't interrupt sysex. 
	   
	   NOTE: active sense messages are not considered to fit under
	   "any" for the purposes of callbacks. If a caller wants
	   active sense messages handled, which is unlikely, then
	   they can just ask for it specifically. They are so unlike
	   every other MIDI message in terms of semantics that its
	   counter-productive to treat them similarly.
	*/
	
	if (inbyte == 0xfe) {
	        message_counter[inbyte]++;
		if (!_offline) {
			active_sense (*this);
		}
		return;
	}
	
	/* If necessary, allocate larger message buffer. */
	
	if (msgindex >= msglen) {
		msglen *= 2;
		msgbuf = (unsigned char *) realloc (msgbuf, msglen);
	}
	
	/*
	  Real time messages can occur ANYPLACE,
	  but do not interrupt running status. 
	*/

	bool rtmsg = false;
	
	switch (inbyte) {
	case 0xf8:
		rtmsg = true;
		break;
	case 0xfa:
		rtmsg = true;
		break;
	case 0xfb:
		rtmsg = true;
		break;
	case 0xfc:
		rtmsg = true;
		break;
	case 0xfd:
		rtmsg = true;
		break;
	case 0xfe:
		rtmsg = true;
		break;
	case 0xff:
		rtmsg = true;
		break;
	}

	if (rtmsg) {
		boost::optional<int> res = edit (&inbyte, 1);
		
		if (res.get_value_or (1) >= 0 && !_offline) {
			realtime_msg (inbyte);
		} 
		
		return;
	} 

	statusbit = (inbyte & 0x80);

	/*
	 * Variable length messages (ie. the 'system exclusive')
	 * can be terminated by the next status byte, not necessarily
	 * an EOX.  Actually, since EOX is a status byte, this
	 * code ALWAYS handles the end of a VARIABLELENGTH message.
	 */

	if (state == VARIABLELENGTH && statusbit)  {

		/* The message has ended, so process it */

		/* add EOX to any sysex message */

		if (inbyte == MIDI::eox) {
			msgbuf[msgindex++] = inbyte;
		}

#if 0
		cerr << "SYSEX: " << hex;
		for (unsigned int i = 0; i < msgindex; ++i) {
			cerr << (int) msgbuf[i] << ' ';
		}
		cerr << dec << endl;
#endif
		if (msgindex > 0) {

			boost::optional<int> res = edit (msgbuf, msgindex);

			if (res.get_value_or (1) >= 0) {
				if (!possible_mmc (msgbuf, msgindex) || _mmc_forward) {
					if (!possible_mtc (msgbuf, msgindex) || _mtc_forward) {
						if (!_offline) {
							sysex (*this, msgbuf, msgindex);
						}
					}
				}
				if (!_offline) {
					any (*this, msgbuf, msgindex);
				} 
			}
		}
	}
	
	/*
	 * Status bytes always start a new message, except EOX
	 */
	
	if (statusbit) {

		msgindex = 0;

		if (inbyte == MIDI::eox) {
			/* return to the state we had pre-sysex */

			state = pre_variable_state;
			runnable = was_runnable;
			msgtype = pre_variable_msgtype;

			if (state != NEEDSTATUS && runnable) {
				msgbuf[msgindex++] = last_status_byte;
			}
		} else {
			msgbuf[msgindex++] = inbyte;
			if ((inbyte & 0xf0) == 0xf0) {
				system_msg (inbyte);
				runnable = false;
			} else {
				channel_msg (inbyte);
			}
		}

		return;
	}
	
	/*
	 * We've got a Data byte.
	 */
	
	msgbuf[msgindex++] = inbyte;

	switch (state) {
	case NEEDSTATUS:
		/*
		 * We shouldn't get here, since in NEEDSTATUS mode
		 * we're expecting a new status byte, NOT any
		 * data bytes. On the other hand, some equipment
		 * with leaky modwheels and the like might be
		 * sending data bytes as part of running controller
		 * messages, so just handle it silently.
		 */
		break;
		
	case NEEDTWOBYTES:
		/* wait for the second byte */
		if (msgindex < 3)
			return;
		/*FALLTHRU*/
		
	case NEEDONEBYTE:
		/* We've completed a 1 or 2 byte message. */

                edit_result = edit (msgbuf, msgindex);
                
		if (edit_result.get_value_or (1)) {
			
			/* message not cancelled by an editor */
			
		        message_counter[msgbuf[0] & 0xF0]++;

			if (!_offline) {
				signal (msgbuf, msgindex);
			}
		}
		
		if (runnable) {
			/* In Runnable mode, we reset the message 
			   index, but keep the callbacks_pending and state the 
			   same.  This provides the "running status 
			   byte" feature.
			*/
			msgindex = 1;
		} else {
			/* If not Runnable, reset to NEEDSTATUS mode */
			state = NEEDSTATUS;
		}
		break;
		
	case VARIABLELENGTH:
		/* nothing to do */
		break;
	}
	return;
}

/** Call the real-time function for the specified byte, immediately.
 * These can occur anywhere, so they don't change the state.
 */
void
Parser::realtime_msg(unsigned char inbyte)

{
	message_counter[inbyte]++;

	if (_offline) {
		return;
	}

	switch (inbyte) {
	case 0xf8:
		timing (*this, _timestamp);
		break;
	case 0xfa:
		start (*this, _timestamp);
		break;
	case 0xfb:
		contineu (*this, _timestamp);
		break;
	case 0xfc:
		stop (*this, _timestamp);
		break;
	case 0xfe:
		/* !!! active sense message in realtime_msg: should not reach here
		 */  
		break;
	case 0xff:
		reset (*this);
		break;
	}

	any (*this, &inbyte, 1);
}


/** Interpret a Channel (voice or mode) Message status byte.
 */
void
Parser::channel_msg(unsigned char inbyte)
{
	last_status_byte = inbyte;
	runnable = true;		/* Channel messages can use running status */
    
	/* The high 4 bits, which determine the type of channel message. */
    
	switch (inbyte&0xF0) {
	case 0x80:
		msgtype = off;
		state = NEEDTWOBYTES;
		break;
	case 0x90:
		msgtype = on;
		state = NEEDTWOBYTES;
		break;
	case 0xa0:
		msgtype = polypress;
		state = NEEDTWOBYTES;
		break;
	case 0xb0:
		msgtype = MIDI::controller;
		state = NEEDTWOBYTES;
		break;
	case 0xc0:
		msgtype = program;
		state = NEEDONEBYTE;
		break;
	case 0xd0:
		msgtype = chanpress;
		state = NEEDONEBYTE;
		break;
	case 0xe0:
		msgtype = MIDI::pitchbend;
		state = NEEDTWOBYTES;
		break;
	}
}

/** Initialize (and possibly emit) the signals for the
 * specified byte.  Set the state that the state-machine
 * should go into.  If the signal is not emitted
 * immediately, it will be when the state machine gets to
 * the end of the MIDI message.
 */
void
Parser::system_msg (unsigned char inbyte)
{
	message_counter[inbyte]++;

	switch (inbyte) {
	case 0xf0:
		pre_variable_msgtype = msgtype;
		pre_variable_state = state;
		was_runnable = runnable;
		msgtype = MIDI::sysex;
		state = VARIABLELENGTH;
		break;
	case 0xf1:
		msgtype = MIDI::mtc_quarter;
		state = NEEDONEBYTE;
		break;
	case 0xf2:
		msgtype = MIDI::position;
		state = NEEDTWOBYTES;
		break;
	case 0xf3:
		msgtype = MIDI::song;
		state = NEEDONEBYTE;
		break;
	case 0xf6:
		if (!_offline) {
			tune (*this);
		}
		state = NEEDSTATUS;
		break;
	case 0xf7:
		break;
	}

	// all these messages will be sent via any() 
	// when they are complete.
	// any (*this, &inbyte, 1);
}

void 
Parser::signal (MIDI::byte *msg, size_t len)
{
	channel_t chan = msg[0]&0xF;
	int chan_i = chan;

	switch (msgtype) {
	case none:
		break;
		
	case off:
		channel_active_preparse[chan_i] (*this);
		note_off (*this, (EventTwoBytes *) &msg[1]);
		channel_note_off[chan_i] 
			(*this, (EventTwoBytes *) &msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case on:
		channel_active_preparse[chan_i] (*this);

		/* Hack to deal with MIDI sources that use velocity=0
		   instead of noteOff.
		*/

		if (msg[2] == 0) {
			note_off (*this, (EventTwoBytes *) &msg[1]);
			channel_note_off[chan_i] 
				(*this, (EventTwoBytes *) &msg[1]);
		} else {
			note_on (*this, (EventTwoBytes *) &msg[1]);
			channel_note_on[chan_i] 
				(*this, (EventTwoBytes *) &msg[1]);
		}

		channel_active_postparse[chan_i] (*this);
		break;
		
	case MIDI::controller:
		channel_active_preparse[chan_i] (*this);
		controller (*this, (EventTwoBytes *) &msg[1]);
		channel_controller[chan_i] 
			(*this, (EventTwoBytes *) &msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case program:
		channel_active_preparse[chan_i] (*this);
		program_change (*this, msg[1]);
		channel_program_change[chan_i] (*this, msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case chanpress:
		channel_active_preparse[chan_i] (*this);
		pressure (*this, msg[1]);
		channel_pressure[chan_i] (*this, msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case polypress:
		channel_active_preparse[chan_i] (*this);
		poly_pressure (*this, (EventTwoBytes *) &msg[1]);
		channel_poly_pressure[chan_i] 
			(*this, (EventTwoBytes *) &msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case MIDI::pitchbend:
		channel_active_preparse[chan_i] (*this);
		pitchbend (*this, (msg[2]<<7)|msg[1]);
		channel_pitchbend[chan_i] (*this, (msg[2]<<7)|msg[1]);
		channel_active_postparse[chan_i] (*this);
		break;
		
	case MIDI::sysex:
		sysex (*this, msg, len);
		break;

	case MIDI::mtc_quarter:
		process_mtc_quarter_frame (msg);
		mtc_quarter_frame (*this, *msg);
		break;
		
	case MIDI::position:
		position (*this, msg, len);
		break;
		
	case MIDI::song:
		song (*this, msg, len);
		break;
	
	case MIDI::tune:
		tune (*this);
	
	default:
		/* XXX some kind of warning ? */
		break;
	}
	
	any (*this, msg, len);
}

bool
Parser::possible_mmc (MIDI::byte *msg, size_t msglen)
{
	if (!MachineControl::is_mmc (msg, msglen)) {
		return false;
	}

	/* hand over the just the interior MMC part of
	   the sysex msg without the leading 0xF0
	*/

	if (!_offline) {
		mmc (*this, &msg[1], msglen - 1);
	}

	return true;
}

void
Parser::set_offline (bool yn)
{
	if (_offline != yn) {
		_offline = yn;
		OfflineStatusChanged ();

		/* this hack deals with the possibility of our first MIDI
		   bytes being running status messages.
		*/
		
		channel_msg (0x90);
		state = NEEDSTATUS;
	}
}

