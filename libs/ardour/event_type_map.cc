/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include <ctype.h>
#include <cstdio>
#include "ardour/types.h"
#include "ardour/event_type_map.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/parameter_types.h"
#include "ardour/uri_map.h"
#include "evoral/Parameter.h"
#include "evoral/ParameterDescriptor.h"
#include "evoral/midi_events.h"
#include "pbd/error.h"
#include "pbd/compose.h"

using namespace std;

namespace ARDOUR {

EventTypeMap* EventTypeMap::event_type_map;

EventTypeMap&
EventTypeMap::instance()
{
	if (!EventTypeMap::event_type_map) {
		EventTypeMap::event_type_map = new EventTypeMap(&URIMap::instance());
	}
	return *EventTypeMap::event_type_map;
}

bool
EventTypeMap::type_is_midi(uint32_t type) const
{
	return ARDOUR::parameter_is_midi((AutomationType)type);
}

uint8_t
EventTypeMap::parameter_midi_type(const Evoral::Parameter& param) const
{
	return ARDOUR::parameter_midi_type((AutomationType)param.type());
}

Evoral::ParameterType
EventTypeMap::midi_parameter_type(const uint8_t* buf, uint32_t len) const
{
	return (uint32_t)ARDOUR::midi_parameter_type(buf[0]);
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
	case MidiNotePressureAutomation:    return Evoral::ControlList::Linear; break;
	case MidiPitchBenderAutomation:     return Evoral::ControlList::Linear; break;
	default: assert(false);
	}
	return Evoral::ControlList::Linear; // Not reached, suppress warnings
}

Evoral::Parameter
EventTypeMap::from_symbol(const string& str) const
{
	AutomationType p_type    = NullAutomation;
	uint8_t        p_channel = 0;
	uint32_t       p_id      = 0;

	if (str == "gain") {
		p_type = GainAutomation;
	} else if (str == "send") {
		p_type = BusSendLevel;
	} else if (str == "trim") {
		p_type = TrimAutomation;
	} else if (str == "main-out-volume") {
		p_type = MainOutVolume;
	} else if (str == "solo") {
		p_type = SoloAutomation;
	} else if (str == "solo-iso") {
		p_type = SoloIsolateAutomation;
	} else if (str == "solo-safe") {
		p_type = SoloSafeAutomation;
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
	} else if (str == "rec-enable") {
		p_type = RecEnableAutomation;
	} else if (str == "rec-safe") {
		p_type = RecSafeAutomation;
	} else if (str == "phase") {
		p_type = PhaseAutomation;
	} else if (str == "monitor") {
		p_type = MonitoringAutomation;
	} else if (str == "pan-lfe") {
		p_type = PanLFEAutomation;
	} else if (str.length() > 10 && str.substr(0, 10) == "parameter-") {
		p_type = PluginAutomation;
		p_id = atoi(str.c_str()+10);
	} else if (str.length() > 9 && str.substr(0, 9) == "property-") {
		p_type = PluginPropertyAutomation;
		const char* name = str.c_str() + 9;
		if (isdigit(str.c_str()[0])) {
			p_id = atoi(name);
		} else {
			p_id = _uri_map->uri_to_id(name);
		}
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
	} else if (str.length() > 19 && str.substr(0, 19) == "midi-note-pressure-") {
		p_type = MidiNotePressureAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-note-pressure-%d-%d", &channel, &p_id);
		assert(channel < 16);
		assert(p_id < 127);
		p_channel = channel;
	} else {
		PBD::warning << "Unknown Parameter '" << str << "'" << endmsg;
	}

	return Evoral::Parameter(p_type, p_channel, p_id);
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
	} else if (t == BusSendLevel) {
		return "send";
	} else if (t == TrimAutomation) {
		return "trim";
	} else if (t == MainOutVolume) {
		return "main-out-volume";
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
	} else if (t == PhaseAutomation) {
		return "phase";
	} else if (t == SoloIsolateAutomation) {
		return "solo-iso";
	} else if (t == SoloSafeAutomation) {
		return "solo-safe";
	} else if (t == MonitoringAutomation) {
		return "monitor";
	} else if (t == RecEnableAutomation) {
		return "rec-enable";
	} else if (t == RecSafeAutomation) {
		return "rec-safe";
	} else if (t == PluginAutomation) {
		return std::string("parameter-") + PBD::to_string(param.id());
	} else if (t == PluginPropertyAutomation) {
		const char* uri = _uri_map->id_to_uri(param.id());
		if (uri) {
			return std::string("property-") + uri;
		} else {
			return std::string("property-") + PBD::to_string(param.id());
		}
	} else if (t == MidiCCAutomation) {
		return std::string("midicc-") + PBD::to_string (param.channel()) + "-" + PBD::to_string (param.id());
	} else if (t == MidiPgmChangeAutomation) {
		return std::string("midi-pgm-change-") + PBD::to_string(param.channel());
	} else if (t == MidiPitchBenderAutomation) {
		return std::string("midi-pitch-bender-") + PBD::to_string(param.channel());
	} else if (t == MidiChannelPressureAutomation) {
		return std::string("midi-channel-pressure-") + PBD::to_string(param.channel());
	} else if (t == MidiNotePressureAutomation) {
		return std::string ("midi-note-pressure-") + PBD::to_string (param.channel()) + "-" + PBD::to_string (param.id());
	} else {
		PBD::warning << "Uninitialized Parameter symbol() called." << endmsg;
		return "";
	}
}

Evoral::ParameterDescriptor
EventTypeMap::descriptor(const Evoral::Parameter& param) const
{
	// Found an existing (perhaps custom) descriptor
	Descriptors::const_iterator d = _descriptors.find(param);
	if (d != _descriptors.end()) {
		return d->second;
	}

	// Add default descriptor and return that
	return ARDOUR::ParameterDescriptor(param);
}

void
EventTypeMap::set_descriptor(const Evoral::Parameter&           param,
                             const Evoral::ParameterDescriptor& desc)
{
	_descriptors.insert(std::make_pair(param, desc));
}

} // namespace ARDOUR

