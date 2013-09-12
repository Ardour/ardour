/*
    Copyright (C) 2009 Paul Davis
 
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

#include <cstring>

#include "midi++/port.h"

#include "midifunction.h"
#include "generic_midi_control_protocol.h"

using namespace MIDI;

MIDIInvokable::MIDIInvokable (MIDI::Parser& p)
	: _parser (p)
{
	data_size = 0;
	data = 0;
}

MIDIInvokable::~MIDIInvokable ()
{
	delete [] data;
}

int
MIDIInvokable::init (GenericMidiControlProtocol& ui, const std::string& name, MIDI::byte* msg_data, size_t data_sz)
{
        _ui = &ui;
        _invokable_name = name;

	if (data_sz) {
		/* we take ownership of the sysex data */
		data = msg_data;
		data_size = data_sz;
	}

	return 0;
}

void
MIDIInvokable::midi_sense_note_on (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, true);
}

void
MIDIInvokable::midi_sense_note_off (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, false);
}

void
MIDIInvokable::midi_sense_note (Parser &, EventTwoBytes *msg, bool /* is_on */)
{
	if (msg->note_number == control_additional) {
		execute ();
	}
}

void
MIDIInvokable::midi_sense_controller (Parser &, EventTwoBytes *msg)
{
	if (control_additional == msg->controller_number) {
		execute ();
	}
}

void
MIDIInvokable::midi_sense_program_change (Parser &, byte msg)
{
	if (msg == control_additional) {
		execute ();
	}
}

void
MIDIInvokable::midi_sense_sysex (Parser &, byte* msg, size_t sz)
{
	if (sz != data_size) {
		return;
	}

	if (memcmp (msg, data, data_size) != 0) {
		return;
	}

	execute ();
}

void
MIDIInvokable::midi_sense_any (Parser &, byte* msg, size_t sz)
{
	if (sz != data_size) {
		return;
	}

	if (memcmp (msg, data, data_size) != 0) {
		return;
	}

	execute ();
}


void
MIDIInvokable::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	midi_sense_connection[0].disconnect ();
	midi_sense_connection[1].disconnect ();

	control_type = ev;
	control_channel = chn;
	control_additional = additional;

	int chn_i = chn;

	/* incoming MIDI is parsed by Ardour' MidiUI event loop/thread, and we want our handlers to execute in that context, so we use
	   Signal::connect_same_thread() here.
	*/

	switch (ev) {
	case MIDI::off:
		_parser.channel_note_off[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_note_off, this, _1, _2));
		break;

	case MIDI::on:
		_parser.channel_note_on[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_note_on, this, _1, _2));
		break;
		
	case MIDI::controller:
		_parser.channel_controller[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_controller, this, _1, _2));
		break;

	case MIDI::program:
		_parser.channel_program_change[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_program_change, this, _1, _2));
		break;

	case MIDI::sysex:
		_parser.sysex.connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_sysex, this, _1, _2, _3));
		break;

	case MIDI::any:
		_parser.any.connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIInvokable::midi_sense_any, this, _1, _2, _3));
		break;

	default:
		break;
	}
}

