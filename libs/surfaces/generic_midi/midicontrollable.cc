/*
    Copyright (C) 1998-2006 Paul Davis
 
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

#include <cstdio> /* for sprintf, sigh */
#include <climits>
#include <pbd/error.h>
#include <pbd/xml++.h>
#include <midi++/port.h>
#include <midi++/channel.h>

#include "midicontrollable.h"

using namespace sigc;
using namespace MIDI;
using namespace PBD;
using namespace ARDOUR;

MIDIControllable::MIDIControllable (Port& p, Controllable& c, bool is_bistate)
	: controllable (c), _port (p), bistate (is_bistate)
{
	setting = false;
	last_value = 0; // got a better idea ?
	control_type = none;
	_control_description = "MIDI Control: none";
	control_additional = (byte) -1;
	connections = 0;
	feedback = true; // for now
	
	/* use channel 0 ("1") as the initial channel */

	midi_rebind (0);
}

MIDIControllable::~MIDIControllable ()
{
	drop_external_control ();
}

void
MIDIControllable::midi_forget ()
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
MIDIControllable::midi_rebind (channel_t c)
{
	if (c >= 0) {
		bind_midi (c, control_type, control_additional);
	} else {
		midi_forget ();
	}
}

void
MIDIControllable::learn_about_external_control ()
{
	drop_external_control ();
	midi_learn_connection = _port.input()->any.connect (mem_fun (*this, &MIDIControllable::midi_receiver));
}

void
MIDIControllable::stop_learning ()
{
	midi_learn_connection.disconnect ();
}

void
MIDIControllable::drop_external_control ()
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
MIDIControllable::midi_sense_note_on (Parser &p, EventTwoBytes *tb) 
{
	midi_sense_note (p, tb, true);
}

void 
MIDIControllable::midi_sense_note_off (Parser &p, EventTwoBytes *tb) 
{
	midi_sense_note (p, tb, false);
}

void
MIDIControllable::midi_sense_note (Parser &p, EventTwoBytes *msg, bool is_on)
{
	if (!bistate) {
		if (msg->note_number == control_additional) {
			controllable.set_value (msg->velocity/127.0);
		}
	} else {

		/* Note: parser handles the use of zero velocity to
		   mean note off. if we get called with is_on=true, then we
		   got a *real* note on.  
		*/

		if (msg->note_number == control_additional) {
			controllable.set_value (is_on ? 1 : 0);
		}
	}

	last_value = (MIDI::byte) (controllable.get_value() * 127.0); // to prevent feedback fights
}

void
MIDIControllable::midi_sense_controller (Parser &, EventTwoBytes *msg)
{
	if (control_additional == msg->controller_number) {
		if (!bistate) {
			controllable.set_value (msg->value/127.0);
		} else {
			if (msg->value > 64.0) {
				controllable.set_value (1);
			} else {
				controllable.set_value (0);
			}
		}

		last_value = (MIDI::byte) (controllable.get_value() * 127.0); // to prevent feedback fights
	}
}

void
MIDIControllable::midi_sense_program_change (Parser &p, byte msg)
{
	/* XXX program change messages make no sense for bistates */

	if (!bistate) {
		controllable.set_value (msg/127.0);
		last_value = (MIDI::byte) (controllable.get_value() * 127.0); // to prevent feedback fights
	} 
}

void
MIDIControllable::midi_sense_pitchbend (Parser &p, pitchbend_t pb)
{
	/* pitchbend messages make no sense for bistates */

	/* XXX gack - get rid of assumption about typeof pitchbend_t */

	controllable.set_value ((pb/(float) SHRT_MAX));
	last_value = (MIDI::byte) (controllable.get_value() * 127.0); // to prevent feedback fights
}			

void
MIDIControllable::midi_receiver (Parser &p, byte *msg, size_t len)
{
	/* we only respond to channel messages */

	if ((msg[0] & 0xF0) < 0x80 || (msg[0] & 0xF0) > 0xE0) {
		return;
	}

	/* if the our port doesn't do input anymore, forget it ... */

	if (!_port.input()) {
		return;
	}

	bind_midi ((channel_t) (msg[0] & 0xf), eventType (msg[0] & 0xF0), msg[1]);

	controllable.LearningFinished ();
}

void
MIDIControllable::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	char buf[64];

	drop_external_control ();

	control_type = ev;
	control_channel = chn;
	control_additional = additional;

	if (_port.input() == 0) {
		return;
	}
	
	Parser& p = *_port.input();

	int chn_i = chn;
	switch (ev) {
	case MIDI::off:
		midi_sense_connection[0] = p.channel_note_off[chn_i].connect
			(mem_fun (*this, &MIDIControllable::midi_sense_note_off));

		/* if this is a bistate, connect to noteOn as well,
		   and we'll toggle back and forth between the two.
		*/

		if (bistate) {
			midi_sense_connection[1] = p.channel_note_on[chn_i].connect
				(mem_fun (*this, &MIDIControllable::midi_sense_note_on));
			connections = 2;
		} else {
			connections = 1;
		}
		_control_description = "MIDI control: NoteOff";
		break;

	case MIDI::on:
		midi_sense_connection[0] = p.channel_note_on[chn_i].connect
			(mem_fun (*this, &MIDIControllable::midi_sense_note_on));
		if (bistate) {
			midi_sense_connection[1] = p.channel_note_off[chn_i].connect 
				(mem_fun (*this, &MIDIControllable::midi_sense_note_off));
			connections = 2;
		} else {
			connections = 1;
		}
		_control_description = "MIDI control: NoteOn";
		break;

	case MIDI::controller:
		midi_sense_connection[0] = p.channel_controller[chn_i].connect 
			(mem_fun (*this, &MIDIControllable::midi_sense_controller));
		connections = 1;
		snprintf (buf, sizeof (buf), "MIDI control: Controller %d", control_additional);
		_control_description = buf;
		break;

	case MIDI::program:
		if (!bistate) {
			midi_sense_connection[0] = p.channel_program_change[chn_i].connect
				(mem_fun (*this, 
				       &MIDIControllable::midi_sense_program_change));
			connections = 1;
			_control_description = "MIDI control: ProgramChange";
		}
		break;

	case MIDI::pitchbend:
		if (!bistate) {
			midi_sense_connection[0] = p.channel_pitchbend[chn_i].connect
				(mem_fun (*this, &MIDIControllable::midi_sense_pitchbend));
			connections = 1;
			_control_description = "MIDI control: Pitchbend";
		}
		break;

	default:
		break;
	}
}

void
MIDIControllable::send_feedback ()
{
	byte msg[3];

	if (setting || !feedback || control_type == none) {
		return;
	}

	msg[0] = (control_type & 0xF0) | (control_channel & 0xF); 
	msg[1] = control_additional;
	msg[2] = (byte) (controllable.get_value() * 127.0f);

	_port.write (msg, 3);
}

MIDI::byte*
MIDIControllable::write_feedback (MIDI::byte* buf, int32_t& bufsize, bool force)
{
	if (control_type != none && feedback && bufsize > 2) {

		MIDI::byte gm = (MIDI::byte) (controllable.get_value() * 127.0);
		
		if (gm != last_value) {
			*buf++ = (0xF0 & control_type) | (0xF & control_channel);
			*buf++ = control_additional; /* controller number */
			*buf++ = gm;
			last_value = gm;
			bufsize -= 3;
		}
	}
	
	return buf;
}

int 
MIDIControllable::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	int xx;

	if ((prop = node.property ("event")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &xx);
		control_type = (MIDI::eventType) xx;
	} else {
		return -1;
	}

	if ((prop = node.property ("channel")) != 0) {
		sscanf (prop->value().c_str(), "%d", &xx);
		control_channel = (MIDI::channel_t) xx;
	} else {
		return -1;
	}

	if ((prop = node.property ("additional")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &xx);
		control_additional = (MIDI::byte) xx;
	} else {
		return -1;
	}

	if ((prop = node.property ("feedback")) != 0) {
		feedback = (prop->value() == "yes");
	} else {
		feedback = true; // default
	}

	bind_midi (control_channel, control_type, control_additional);
	
	return 0;
}

XMLNode&
MIDIControllable::get_state ()
{
	char buf[32];
	XMLNode& node (controllable.get_state ());

	snprintf (buf, sizeof(buf), "0x%x", (int) control_type);
	node.add_property ("event", buf);
	snprintf (buf, sizeof(buf), "%d", (int) control_channel);
	node.add_property ("channel", buf);
	snprintf (buf, sizeof(buf), "0x%x", (int) control_additional);
	node.add_property ("additional", buf);
	node.add_property ("feedback", (feedback ? "yes" : "no"));

	return node;
}

