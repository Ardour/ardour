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

    $Id$
*/

#include <cstdio> /* for sprintf, sigh */
#include <pbd/error.h>
#include <midi++/port.h>
#include <midi++/channel.h>
#include <midi++/controllable.h>

using namespace sigc;
using namespace MIDI;
using namespace PBD;

Controllable::Controllable (Port *p, bool is_bistate)
{
	control_type = none;
	_control_description = "MIDI Control: none";
	control_additional = (byte) -1;
	bistate = is_bistate;
	connections = 0;
	feedback = true; // for now
	
	/* use channel 0 ("1") as the initial channel */

	midi_rebind (p, 0);
}

Controllable::~Controllable ()
{
	drop_external_control ();
}

void
Controllable::midi_forget ()
{
	/* stop listening for incoming messages, but retain
	   our existing event + type information.
	*/
	
	if (connections > 0) {
		midi_sense_connection[0].disconnect ();
	} 
	
	if (connections > 1) {
		midi_sense_connection[1].disconnect ();
	}
	
	connections = 0;
	midi_learn_connection.disconnect ();
	
}

void
Controllable::midi_rebind (Port *p, channel_t c)
{
	if ((port = p) == 0) {
		midi_forget ();
	} else {
		if (c >= 0) {
			bind_midi (c, control_type, control_additional);
		} else {
			midi_forget ();
		}
	}
}

void
Controllable::learn_about_external_control ()
{
	drop_external_control ();

	if (port) {
		midi_learn_connection = port->input()->any.connect (mem_fun (*this, &Controllable::midi_receiver));
		learning_started ();

	} else {
		info << "No MIDI port specified - external control disabled" << endmsg;
	}
}

void
Controllable::stop_learning ()
{
	midi_learn_connection.disconnect ();
}

void
Controllable::drop_external_control ()
{
	if (connections > 0) {
		midi_sense_connection[0].disconnect ();
	} 
	if (connections > 1) {
		midi_sense_connection[1].disconnect ();
	}

	connections = 0;
	midi_learn_connection.disconnect ();

	control_type = none;
	control_additional = (byte) -1;
}

void 
Controllable::midi_sense_note_on (Parser &p, EventTwoBytes *tb) 
{
	midi_sense_note (p, tb, true);
}

void 
Controllable::midi_sense_note_off (Parser &p, EventTwoBytes *tb) 
{
	midi_sense_note (p, tb, false);
}

void
Controllable::midi_sense_note (Parser &p, EventTwoBytes *msg, bool is_on)
{
	if (!bistate) {
		set_value (msg->note_number/127.0);
	} else {

		/* Note: parser handles the use of zero velocity to
		   mean note off. if we get called with is_on=true, then we
		   got a *real* note on.  
		*/

		if (msg->note_number == control_additional) {
			set_value (is_on ? 1 : 0);
		}
	}
}

void
Controllable::midi_sense_controller (Parser &, EventTwoBytes *msg)
{
	if (control_additional == msg->controller_number) {
		if (!bistate) {
			set_value (msg->value/127.0);
		} else {
			if (msg->value > 64.0) {
				set_value (1);
			} else {
				set_value (0);
			}
		}
	}
}

void
Controllable::midi_sense_program_change (Parser &p, byte msg)
{
	/* XXX program change messages make no sense for bistates */

	if (!bistate) {
		set_value (msg/127.0);
	} 
}

void
Controllable::midi_sense_pitchbend (Parser &p, pitchbend_t pb)
{
	/* pitchbend messages make no sense for bistates */

	/* XXX gack - get rid of assumption about typeof pitchbend_t */

	set_value ((pb/(float) SHRT_MAX));
}			

void
Controllable::midi_receiver (Parser &p, byte *msg, size_t len)
{
	/* we only respond to channel messages */

	if ((msg[0] & 0xF0) < 0x80 || (msg[0] & 0xF0) > 0xE0) {
		return;
	}

	/* if the our port doesn't do input anymore, forget it ... */

	if (!port->input()) {
		return;
	}

	bind_midi ((channel_t) (msg[0] & 0xf), eventType (msg[0] & 0xF0), msg[1]);

	learning_stopped ();
}

void
Controllable::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	char buf[64];

	drop_external_control ();

	control_type = ev;
	control_channel = chn;
	control_additional = additional;

	if (port == 0 || port->input() == 0) {
		return;
	}
	
	Parser& p = *port->input();

	int chn_i = chn;
	switch (ev) {
	case MIDI::off:
		midi_sense_connection[0] = p.channel_note_off[chn_i].connect
			(mem_fun (*this, &Controllable::midi_sense_note_off));

		/* if this is a bistate, connect to noteOn as well,
		   and we'll toggle back and forth between the two.
		*/

		if (bistate) {
			midi_sense_connection[1] = p.channel_note_on[chn_i].connect
				(mem_fun (*this, &Controllable::midi_sense_note_on));
			connections = 2;
		} else {
			connections = 1;
		}
		_control_description = "MIDI control: NoteOff";
		break;

	case MIDI::on:
		midi_sense_connection[0] = p.channel_note_on[chn_i].connect
			(mem_fun (*this, &Controllable::midi_sense_note_on));
		if (bistate) {
			midi_sense_connection[1] = p.channel_note_off[chn_i].connect 
				(mem_fun (*this, &Controllable::midi_sense_note_off));
			connections = 2;
		} else {
			connections = 1;
		}
		_control_description = "MIDI control: NoteOn";
		break;

	case MIDI::controller:
		midi_sense_connection[0] = p.channel_controller[chn_i].connect 
			(mem_fun (*this, &Controllable::midi_sense_controller));
		connections = 1;
		snprintf (buf, sizeof (buf), "MIDI control: Controller %d", control_additional);
		_control_description = buf;
		break;

	case MIDI::program:
		if (!bistate) {
			midi_sense_connection[0] = p.channel_program_change[chn_i].connect
				(mem_fun (*this, 
				       &Controllable::midi_sense_program_change));
			connections = 1;
			_control_description = "MIDI control: ProgramChange";
		}
		break;

	case MIDI::pitchbend:
		if (!bistate) {
			midi_sense_connection[0] = p.channel_pitchbend[chn_i].connect
				(mem_fun (*this, &Controllable::midi_sense_pitchbend));
			connections = 1;
			_control_description = "MIDI control: Pitchbend";
		}
		break;

	default:
		break;
	}
}

void
Controllable::set_control_type (channel_t chn, eventType ev, MIDI::byte additional)
{
	bind_midi (chn, ev, additional);
}

bool
Controllable::get_control_info (channel_t& chn, eventType& ev, byte& additional)
{
	if (control_type == none) {
		chn = -1;
		return false;
	} 
	
	ev = control_type;
	chn = control_channel;
	additional = control_additional;

	return true;
}


void
Controllable::send_midi_feedback (float val, timestamp_t timestamp)
{
	byte msg[3];

	if (port == 0 || control_type == none) {
		return;
	}
	
	msg[0] = (control_type & 0xF0) | (control_channel & 0xF); 
	msg[1] = control_additional;
	msg[2] = (byte) (val * 127.0f);

	port->write (msg, 3, timestamp);
}

