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
	: MIDIInvokable (p)
{
}

MIDIFunction::~MIDIFunction ()
{
}

int
MIDIFunction::init (GenericMidiControlProtocol& ui, const std::string& invokable_name, MIDI::byte* msg_data, size_t data_sz)
{
        MIDIInvokable::init (ui, invokable_name, msg_data, data_sz);

	if (strcasecmp (_invokable_name.c_str(), "transport-stop") == 0) {
		_function = TransportStop;
	} else if (strcasecmp (_invokable_name.c_str(), "transport-roll") == 0) {
		_function = TransportRoll;
	} else if (strcasecmp (_invokable_name.c_str(), "transport-zero") == 0) {
		_function = TransportZero;
	} else if (strcasecmp (_invokable_name.c_str(), "transport-start") == 0) {
		_function = TransportStart;
	} else if (strcasecmp (_invokable_name.c_str(), "transport-end") == 0) {
		_function = TransportEnd;
	} else if (strcasecmp (_invokable_name.c_str(), "loop-toggle") == 0) {
		_function = TransportLoopToggle;
	} else if (strcasecmp (_invokable_name.c_str(), "rec-enable") == 0) {
		_function = TransportRecordEnable;
	} else if (strcasecmp (_invokable_name.c_str(), "rec-disable") == 0) {
		_function = TransportRecordDisable;
	} else if (strcasecmp (_invokable_name.c_str(), "next-bank") == 0) {
		_function = NextBank;
	} else if (strcasecmp (_invokable_name.c_str(), "prev-bank") == 0) {
		_function = PrevBank;
	} else {
		return -1;
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

