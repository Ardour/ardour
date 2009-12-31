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

MIDIFunction::MIDIFunction (MIDI::Port& p)
	: _port (p)
{
	sysex_size = 0;
	sysex = 0;
}

MIDIFunction::~MIDIFunction ()
{
	delete [] sysex;
}

int
MIDIFunction::init (GenericMidiControlProtocol& ui, const std::string& function_name, MIDI::byte* sysex_data, size_t sysex_sz)
{
	if (strcasecmp (function_name.c_str(), "transport-stop") == 0) {
		_function = TransportStop;
	} else if (strcasecmp (function_name.c_str(), "transport-roll") == 0) {
		_function = TransportRoll;
	} else if (strcasecmp (function_name.c_str(), "transport-zero") == 0) {
		_function = TransportZero;
	} else if (strcasecmp (function_name.c_str(), "transport-start") == 0) {
		_function = TransportStart;
	} else if (strcasecmp (function_name.c_str(), "transport-end") == 0) {
		_function = TransportEnd;
	} else if (strcasecmp (function_name.c_str(), "loop-toggle") == 0) {
		_function = TransportLoopToggle;
	} else if (strcasecmp (function_name.c_str(), "rec-enable") == 0) {
		_function = TransportRecordEnable;
	} else if (strcasecmp (function_name.c_str(), "rec-disable") == 0) {
		_function = TransportRecordDisable;
	} else if (strcasecmp (function_name.c_str(), "next-bank") == 0) {
		_function = NextBank;
	} else if (strcasecmp (function_name.c_str(), "prev-bank") == 0) {
		_function = PrevBank;
	} else {
		return -1;
	}

	_ui = &ui;

	if (sysex_sz) {
		/* we take ownership of the sysex data */
		sysex = sysex_data;
		sysex_size = sysex_sz;
	}

	return 0;
}

void
MIDIFunction::execute ()
{
	switch (_function) {
	case NextBank:
		_ui->next_bank();
		break;

	case PrevBank:
		_ui->prev_bank();
		break;

	case TransportStop:
		_ui->transport_stop ();
		break;

	case TransportRoll:
		_ui->transport_play ();
		break;

	case TransportStart:
		_ui->goto_start ();
		break;

	case TransportZero:
		// need this in BasicUI
		break;

	case TransportEnd:
		_ui->goto_end ();
		break;

	case TransportLoopToggle:
		_ui->loop_toggle ();
		break;

	case TransportRecordEnable:
		_ui->set_record_enable (true);
		break;

	case TransportRecordDisable:
		_ui->set_record_enable (false);
		break;
	}
}

void
MIDIFunction::midi_sense_note_on (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, true);
}

void
MIDIFunction::midi_sense_note_off (Parser &p, EventTwoBytes *tb)
{
	midi_sense_note (p, tb, false);
}

void
MIDIFunction::midi_sense_note (Parser &, EventTwoBytes *msg, bool /* is_on */)
{
	if (msg->note_number == control_additional) {
		execute ();
	}
}

void
MIDIFunction::midi_sense_controller (Parser &, EventTwoBytes *msg)
{
	if (control_additional == msg->controller_number) {
		execute ();
	}
}

void
MIDIFunction::midi_sense_program_change (Parser &, byte msg)
{
	if (msg == control_additional) {
		execute ();
	}
}

void
MIDIFunction::midi_sense_sysex (Parser &, byte* msg, size_t sz)
{
	if (sz != sysex_size) {
		return;
	}

	if (memcmp (msg, sysex, sysex_size) != 0) {
		return;
	}

	execute ();
}

void
MIDIFunction::bind_midi (channel_t chn, eventType ev, MIDI::byte additional)
{
	midi_sense_connection[0].disconnect ();
	midi_sense_connection[1].disconnect ();

	control_type = ev;
	control_channel = chn;
	control_additional = additional;

	if (_port.input() == 0) {
		return;
	}

	Parser& p = *_port.input();

	int chn_i = chn;

	/* incoming MIDI is parsed by Ardour' MidiUI event loop/thread, and we want our handlers to execute in that context, so we use
	   Signal::connect_same_thread() here.
	*/

	switch (ev) {
	case MIDI::off:
		p.channel_note_off[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIFunction::midi_sense_note_off, this, _1, _2));
		break;

	case MIDI::on:
		p.channel_note_on[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIFunction::midi_sense_note_on, this, _1, _2));
		break;
		
	case MIDI::controller:
		p.channel_controller[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIFunction::midi_sense_controller, this, _1, _2));
		break;

	case MIDI::program:
		p.channel_program_change[chn_i].connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIFunction::midi_sense_program_change, this, _1, _2));
		break;

	case MIDI::sysex:
		p.sysex.connect_same_thread (midi_sense_connection[0], boost::bind (&MIDIFunction::midi_sense_sysex, this, _1, _2, _3));
		break;

	default:
		break;
	}
}

XMLNode&
MIDIFunction::get_state ()
{
	XMLNode* node = new XMLNode ("MIDIFunction");
	return *node;
}

int
MIDIFunction::set_state (const XMLNode& node, int version)
{
	return 0;
}

