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
	: _type(NullAutomation)
	, _id(0)
	, _channel(0)
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
		cout << "LOADED PARAMETER " << str << " chan " << _channel << " id " << _id << endl; 
		//_id = atoi(str.c_str()+7);
	} else {
		PBD::warning << "Unknown Parameter '" << str << "'" << endmsg;
	}
}


/** Unique string representation, suitable as an XML property value.
 * e.g. <AutomationList automation-id="whatthisreturns">
 */
std::string
Parameter::to_string() const
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
		return string_compose("midicc-%1-%2", _channel, _id);
	} else {
		assert(false);
		PBD::warning << "Uninitialized Parameter to_string() called." << endmsg;
		return "";
	}
}

