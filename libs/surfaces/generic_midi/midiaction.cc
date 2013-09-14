/*
    Copyright (C) 2009-2010 Paul Davis
 
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

#include "midiaction.h"
#include "generic_midi_control_protocol.h"

using namespace MIDI;

MIDIAction::MIDIAction (MIDI::Parser& p)
	: MIDIInvokable (p)
{
}

MIDIAction::~MIDIAction ()
{
}

int
MIDIAction::init (GenericMidiControlProtocol& ui, const std::string& invokable_name, MIDI::byte* msg_data, size_t data_sz)
{
        MIDIInvokable::init (ui, invokable_name, msg_data, data_sz);
	return 0;
}

void
MIDIAction::execute ()
{
        _ui->access_action (_invokable_name);
}

XMLNode&
MIDIAction::get_state ()
{
	XMLNode* node = new XMLNode ("MIDIAction");
	return *node;
}

int
MIDIAction::set_state (const XMLNode& /*node*/, int /*version*/)
{
	return 0;
}

