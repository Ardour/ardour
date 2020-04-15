/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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

#ifndef __ardour_parameter_types_h__
#define __ardour_parameter_types_h__

#include <stdint.h>

#include "ardour/types.h"
#include "evoral/Parameter.h"
#include "evoral/midi_events.h"

namespace ARDOUR {

inline uint8_t
parameter_midi_type(AutomationType type)
{
	switch (type) {
	case MidiCCAutomation:              return MIDI_CMD_CONTROL;          break;
	case MidiPgmChangeAutomation:       return MIDI_CMD_PGM_CHANGE;       break;
	case MidiChannelPressureAutomation: return MIDI_CMD_CHANNEL_PRESSURE; break;
	case MidiNotePressureAutomation:    return MIDI_CMD_NOTE_PRESSURE;    break;
	case MidiPitchBenderAutomation:     return MIDI_CMD_BENDER;           break;
	case MidiSystemExclusiveAutomation: return MIDI_CMD_COMMON_SYSEX;     break;
	default: return 0;
	}
}

inline AutomationType
midi_parameter_type(uint8_t status)
{
	switch (status & 0xF0) {
	case MIDI_CMD_CONTROL:          return MidiCCAutomation;              break;
	case MIDI_CMD_PGM_CHANGE:       return MidiPgmChangeAutomation;       break;
	case MIDI_CMD_CHANNEL_PRESSURE: return MidiChannelPressureAutomation; break;
	case MIDI_CMD_NOTE_PRESSURE:    return MidiNotePressureAutomation;    break;
	case MIDI_CMD_BENDER:           return MidiPitchBenderAutomation;     break;
	case MIDI_CMD_COMMON_SYSEX:     return MidiSystemExclusiveAutomation; break;
	default: return NullAutomation;
	}
}

inline Evoral::Parameter
midi_parameter(const uint8_t* buf, const uint32_t len)
{
	const uint8_t channel = buf[0] & 0x0F;
	switch (midi_parameter_type(buf[0])) {
	case MidiCCAutomation:
		return Evoral::Parameter(MidiCCAutomation, channel, buf[1]);
	case MidiPgmChangeAutomation:
		return Evoral::Parameter(MidiPgmChangeAutomation, channel);
	case MidiChannelPressureAutomation:
		return Evoral::Parameter(MidiChannelPressureAutomation, channel);
	case MidiNotePressureAutomation:
		return Evoral::Parameter(MidiNotePressureAutomation, channel);
	case MidiPitchBenderAutomation:
		return Evoral::Parameter(MidiPitchBenderAutomation, channel);
	case MidiSystemExclusiveAutomation:
		return Evoral::Parameter(MidiSystemExclusiveAutomation, channel);
	default:
		return Evoral::Parameter(NullAutomation);
	}
}

inline bool
parameter_is_midi(AutomationType type)
{
	return (type >= MidiCCAutomation) && (type <= MidiNotePressureAutomation);
}

inline bool
parameter_is_midi(Evoral::ParameterType t)
{
	AutomationType type = (AutomationType) t;
	return (type >= MidiCCAutomation) && (type <= MidiNotePressureAutomation);
}

}  // namespace ARDOUR

#endif /* __ardour_parameter_types_h__ */

