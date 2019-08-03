/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstring>

#include "midi++/port.h"
#include "pbd/compose.h"
#include "ardour/debug.h"

#include "midiaction.h"
#include "generic_midi_control_protocol.h"

using namespace MIDI;
using namespace PBD;

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
        DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Action: '%1'\n", _invokable_name));
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

