/*
    Copyright (C) 2008 Paul Davis
    Author: David Robillard

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

#include <cstdio>
#include "ardour/types.h"
#include "ardour/event_type_map.h"
#include "evoral/Parameter.hpp"
#include "evoral/midi_events.h"
#include "evoral/MIDIParameters.hpp"
#include "pbd/error.h"
#include "pbd/compose.h"

using namespace std;

namespace ARDOUR {

EventTypeMap EventTypeMap::event_type_map;

bool
EventTypeMap::type_is_midi(uint32_t type) const
{
	return (type >= MidiCCAutomation) && (type <= MidiChannelPressureAutomation);
}

bool
EventTypeMap::is_midi_parameter(const Evoral::Parameter& param)
{
		return type_is_midi(param.type());
}

uint8_t
EventTypeMap::parameter_midi_type(const Evoral::Parameter& param) const
{
	switch (param.type()) {
	case MidiCCAutomation:              return MIDI_CMD_CONTROL; break;
	case MidiPgmChangeAutomation:       return MIDI_CMD_PGM_CHANGE; break;
	case MidiChannelPressureAutomation: return MIDI_CMD_CHANNEL_PRESSURE; break;
	case MidiPitchBenderAutomation:     return MIDI_CMD_BENDER; break;
	case MidiSystemExclusiveAutomation: return MIDI_CMD_COMMON_SYSEX; break;
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
	case MIDI_CMD_COMMON_SYSEX:     return MidiSystemExclusiveAutomation; break;
	default: return 0;
	}
}

bool
EventTypeMap::is_integer(const Evoral::Parameter& param) const
{
	return (   param.type() >= MidiCCAutomation
			&& param.type() <= MidiChannelPressureAutomation);
}

Evoral::ControlList::InterpolationStyle
EventTypeMap::interpolation_of(const Evoral::Parameter& param)
{
	switch (param.type()) {
	case MidiCCAutomation:
		switch (param.id()) {
		case MIDI_CTL_LSB_BANK:
		case MIDI_CTL_MSB_BANK:
		case MIDI_CTL_LSB_EFFECT1:
		case MIDI_CTL_LSB_EFFECT2:
		case MIDI_CTL_MSB_EFFECT1:
		case MIDI_CTL_MSB_EFFECT2:
		case MIDI_CTL_MSB_GENERAL_PURPOSE1:
		case MIDI_CTL_MSB_GENERAL_PURPOSE2:
		case MIDI_CTL_MSB_GENERAL_PURPOSE3:
		case MIDI_CTL_MSB_GENERAL_PURPOSE4:
		case MIDI_CTL_SUSTAIN:
		case MIDI_CTL_PORTAMENTO:
		case MIDI_CTL_SOSTENUTO:
		case MIDI_CTL_SOFT_PEDAL:
		case MIDI_CTL_LEGATO_FOOTSWITCH:
		case MIDI_CTL_HOLD2:
		case MIDI_CTL_GENERAL_PURPOSE5:
		case MIDI_CTL_GENERAL_PURPOSE6:
		case MIDI_CTL_GENERAL_PURPOSE7:
		case MIDI_CTL_GENERAL_PURPOSE8:
		case MIDI_CTL_DATA_INCREMENT:
		case MIDI_CTL_DATA_DECREMENT:
		case MIDI_CTL_NONREG_PARM_NUM_LSB:
		case MIDI_CTL_NONREG_PARM_NUM_MSB:
		case MIDI_CTL_REGIST_PARM_NUM_LSB:
		case MIDI_CTL_REGIST_PARM_NUM_MSB:
		case MIDI_CTL_ALL_SOUNDS_OFF:
		case MIDI_CTL_RESET_CONTROLLERS:
		case MIDI_CTL_LOCAL_CONTROL_SWITCH:
		case MIDI_CTL_ALL_NOTES_OFF:
		case MIDI_CTL_OMNI_OFF:
		case MIDI_CTL_OMNI_ON:
		case MIDI_CTL_MONO:
		case MIDI_CTL_POLY:
			return Evoral::ControlList::Discrete; break;
		default:
			return Evoral::ControlList::Linear; break;
		}
		break;
	case MidiPgmChangeAutomation:       return Evoral::ControlList::Discrete; break;
	case MidiChannelPressureAutomation: return Evoral::ControlList::Linear; break;
	case MidiPitchBenderAutomation:     return Evoral::ControlList::Linear; break;
	default: assert(false);
	}
	return Evoral::ControlList::Linear; // Not reached, suppress warnings
}


Evoral::Parameter
EventTypeMap::new_parameter(uint32_t type, uint8_t channel, uint32_t id) const
{
	Evoral::Parameter p(type, channel, id);

	double min    = 0.0f;
	double max    = 1.0f;
	double normal = 0.0f;

	switch((AutomationType)type) {
	case NullAutomation:
	case GainAutomation:
		max = 2.0f;
		normal = 1.0f;
		break;
	case PanAzimuthAutomation:
		normal = 0.5f; // there really is no normal but this works for stereo, sort of
                break;
	case PanWidthAutomation:
                min = -1.0;
                max = 1.0;
		normal = 0.0f;
                break;
        case PanElevationAutomation:
        case PanFrontBackAutomation:
        case PanLFEAutomation:
                break;
	case RecEnableAutomation:
		/* default 0.0 - 1.0 is fine */
		break;
	case PluginAutomation:
	case SoloAutomation:
	case MuteAutomation:
	case FadeInAutomation:
	case FadeOutAutomation:
	case EnvelopeAutomation:
		max = 2.0f;
		normal = 1.0f;
		break;
	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiChannelPressureAutomation:
		Evoral::MIDI::controller_range(min, max, normal); break;
	case MidiPitchBenderAutomation:
		Evoral::MIDI::bender_range(min, max, normal); break;
	case MidiSystemExclusiveAutomation:
		return p;
	}

	p.set_range(type, min, max, normal, false);
	return p;
}

Evoral::Parameter
EventTypeMap::new_parameter(const string& str) const
{
	AutomationType p_type    = NullAutomation;
	uint8_t        p_channel = 0;
	uint32_t       p_id      = 0;

	if (str == "gain") {
		p_type = GainAutomation;
	} else if (str == "solo") {
		p_type = SoloAutomation;
	} else if (str == "mute") {
		p_type = MuteAutomation;
	} else if (str == "fadein") {
		p_type = FadeInAutomation;
	} else if (str == "fadeout") {
		p_type = FadeOutAutomation;
	} else if (str == "envelope") {
		p_type = EnvelopeAutomation;
	} else if (str == "pan-azimuth") {
		p_type = PanAzimuthAutomation;
	} else if (str == "pan-width") {
		p_type = PanWidthAutomation;
	} else if (str == "pan-elevation") {
		p_type = PanElevationAutomation;
	} else if (str == "pan-frontback") {
		p_type = PanFrontBackAutomation;
	} else if (str == "pan-lfe") {
		p_type = PanLFEAutomation;
	} else if (str.length() > 10 && str.substr(0, 10) == "parameter-") {
		p_type = PluginAutomation;
		p_id = atoi(str.c_str()+10);
	} else if (str.length() > 7 && str.substr(0, 7) == "midicc-") {
		p_type = MidiCCAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midicc-%d-%d", &channel, &p_id);
		assert(channel < 16);
		p_channel = channel;
	} else if (str.length() > 16 && str.substr(0, 16) == "midi-pgm-change-") {
		p_type = MidiPgmChangeAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-pgm-change-%d", &channel);
		assert(channel < 16);
		p_id = 0;
		p_channel = channel;
	} else if (str.length() > 18 && str.substr(0, 18) == "midi-pitch-bender-") {
		p_type = MidiPitchBenderAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-pitch-bender-%d", &channel);
		assert(channel < 16);
		p_id = 0;
		p_channel = channel;
	} else if (str.length() > 22 && str.substr(0, 22) == "midi-channel-pressure-") {
		p_type = MidiChannelPressureAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-channel-pressure-%d", &channel);
		assert(channel < 16);
		p_id = 0;
		p_channel = channel;
	} else {
		PBD::warning << "Unknown Parameter '" << str << "'" << endmsg;
	}
	
	return new_parameter(p_type, p_channel, p_id);
}

/** Unique string representation, suitable as an XML property value.
 * e.g. <AutomationList automation-id="whatthisreturns">
 */
std::string
EventTypeMap::to_symbol(const Evoral::Parameter& param) const
{
	AutomationType t = (AutomationType)param.type();

	if (t == GainAutomation) {
		return "gain";
	} else if (t == PanAzimuthAutomation) {
                return "pan-azimuth";
	} else if (t == PanElevationAutomation) {
                return "pan-elevation";
	} else if (t == PanWidthAutomation) {
                return "pan-width";
	} else if (t == PanFrontBackAutomation) {
                return "pan-frontback";
	} else if (t == PanLFEAutomation) {
                return "pan-lfe";
	} else if (t == SoloAutomation) {
		return "solo";
	} else if (t == MuteAutomation) {
		return "mute";
	} else if (t == FadeInAutomation) {
		return "fadein";
	} else if (t == FadeOutAutomation) {
		return "fadeout";
	} else if (t == EnvelopeAutomation) {
		return "envelope";
	} else if (t == PluginAutomation) {
		return string_compose("parameter-%1", param.id());
	} else if (t == MidiCCAutomation) {
		return string_compose("midicc-%1-%2", int(param.channel()), param.id());
	} else if (t == MidiPgmChangeAutomation) {
		return string_compose("midi-pgm-change-%1", int(param.channel()));
	} else if (t == MidiPitchBenderAutomation) {
		return string_compose("midi-pitch-bender-%1", int(param.channel()));
	} else if (t == MidiChannelPressureAutomation) {
		return string_compose("midi-channel-pressure-%1", int(param.channel()));
	} else {
		PBD::warning << "Uninitialized Parameter symbol() called." << endmsg;
		return "";
	}
}

} // namespace ARDOUR

