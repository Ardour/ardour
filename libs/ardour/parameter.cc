/*
    Copyright (C) 2008 Paul Davis 
    Written by Dave Robillard

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

#include <ardour/parameter.h>

using namespace ARDOUR;
	

/** Construct an Parameter from a string returned from Parameter::to_string
 * (AutomationList automation-id property)
 */
Parameter::Parameter(const std::string& str)
	: Evoral::Parameter (NullAutomation, 0)
{
	if (str == "gain") {
		_type = GainAutomation;
	} else if (str == "solo") {
		_type = SoloAutomation;
	} else if (str == "mute") {
		_type = MuteAutomation;
	} else if (str == "fadein") {
		_type = FadeInAutomation;
	} else if (str == "fadeout") {
		_type = FadeOutAutomation;
	} else if (str == "envelope") {
		_type = EnvelopeAutomation;
	} else if (str == "pan") {
		_type = PanAutomation;
	} else if (str.length() > 4 && str.substr(0, 4) == "pan-") {
		_type = PanAutomation;
		_id = atoi(str.c_str()+4);
	} else if (str.length() > 10 && str.substr(0, 10) == "parameter-") {
		_type = PluginAutomation;
		_id = atoi(str.c_str()+10);
	} else if (str.length() > 7 && str.substr(0, 7) == "midicc-") {
		_type = MidiCCAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midicc-%d-%d", &channel, &_id);
		assert(channel < 16);
		_channel = channel;
	} else if (str.length() > 16 && str.substr(0, 16) == "midi-pgm-change-") {
		_type = MidiPgmChangeAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-pgm-change-%d", &channel);
		assert(channel < 16);
		_id = 0;
		_channel = channel;
	} else if (str.length() > 18 && str.substr(0, 18) == "midi-pitch-bender-") {
		_type = MidiPitchBenderAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-pitch-bender-%d", &channel);
		assert(channel < 16);
		_id = 0;
		_channel = channel;
	} else if (str.length() > 24 && str.substr(0, 24) == "midi-channel-aftertouch-") {
		_type = MidiChannelAftertouchAutomation;
		uint32_t channel = 0;
		sscanf(str.c_str(), "midi-channel-aftertouch-%d", &channel);
		assert(channel < 16);
		_id = 0;
		_channel = channel;
	} else {
		PBD::warning << "Unknown Parameter '" << str << "'" << endmsg;
	}

	init_metadata((AutomationType)_type); // set min/max/normal
}


/** Unique string representation, suitable as an XML property value.
 * e.g. <AutomationList automation-id="whatthisreturns">
 */
std::string
Parameter::symbol() const
{
	if (_type == GainAutomation) {
		return "gain";
	} else if (_type == PanAutomation) {
		return string_compose("pan-%1", _id);
	} else if (_type == SoloAutomation) {
		return "solo";
	} else if (_type == MuteAutomation) {
		return "mute";
	} else if (_type == FadeInAutomation) {
		return "fadein";
	} else if (_type == FadeOutAutomation) {
		return "fadeout";
	} else if (_type == EnvelopeAutomation) {
		return "envelope";
	} else if (_type == PluginAutomation) {
		return string_compose("parameter-%1", _id);
	} else if (_type == MidiCCAutomation) {
		return string_compose("midicc-%1-%2", int(_channel), _id);
	} else if (_type == MidiPgmChangeAutomation) {
		return string_compose("midi-pgm-change-%1", int(_channel));
	} else if (_type == MidiPitchBenderAutomation) {
		return string_compose("midi-pitch-bender-%1", int(_channel));
	} else if (_type == MidiChannelAftertouchAutomation) {
		return string_compose("midi-channel-aftertouch-%1", int(_channel));
	} else {
		PBD::warning << "Uninitialized Parameter symbol() called." << endmsg;
		return "";
	}
}

