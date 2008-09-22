/*
    Copyright (C) 2008 Paul Davis
    Author: Dave Robillard

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

#include <ardour/types.h>
#include <ardour/event_type_map.h>
#include <evoral/Parameter.hpp>
#include <evoral/midi_events.h>

namespace ARDOUR {

EventTypeMap EventTypeMap::event_type_map;

bool
EventTypeMap::type_is_midi(uint32_t type) const
{
	return (type >= MidiCCAutomation) && (type <= MidiChannelPressureAutomation);
}

uint8_t
EventTypeMap::parameter_midi_type(const Evoral::Parameter& param) const
{
	switch (param.type()) {
	case MidiCCAutomation:              return MIDI_CMD_CONTROL; break; 
	case MidiPgmChangeAutomation:       return MIDI_CMD_PGM_CHANGE; break; 
	case MidiChannelPressureAutomation: return MIDI_CMD_CHANNEL_PRESSURE; break; 
	case MidiPitchBenderAutomation:     return MIDI_CMD_BENDER; break; 
	default: return 0;
	}
}

uint32_t
EventTypeMap::midi_event_type(uint8_t status) const
{
	switch (status & 0xF0) {
	case MIDI_CMD_CONTROL:          return MidiCCAutomation; break;
	case MIDI_CMD_PGM_CHANGE:       return MidiPgmChangeAutomation; break;
	case MIDI_CMD_CHANNEL_PRESSURE: return MidiChannelPressureAutomation; break;
	case MIDI_CMD_BENDER:           return MidiPitchBenderAutomation; break;
	default: return 0;
	}
}

} // namespace ARDOUR

