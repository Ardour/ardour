/*
    Copyright (C) 2007 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_param_id_h__
#define __ardour_param_id_h__

#include <string>
#include <pbd/compose.h>
#include <pbd/error.h>
#include <ardour/types.h>

namespace ARDOUR {


/** ID of an automatable parameter.
 *
 * A given automatable object has a number of automatable parameters.  This is
 * the unique ID for those parameters.  Anything automatable (AutomationList,
 * Curve) must have an ID unique with respect to it's Automatable parent.
 *
 * A parameter ID has two parts, a type and an int (only used by some types).
 *
 * This is a bit more ugly than it could be, due to using the existing/legacy
 * ARDOUR::AutomationType:  GainAutomation, PanAutomation, SoloAutomation,
 * and MuteAutomation use only the type(), but PluginAutomation and
 * MidiCCAutomation use the id() as port number and CC number, respectively.
 *
 * Future types may use a string or URI or whatever, as long as these are
 * comparable anything may be added.  ints are best as these should be fast to
 * copy and compare with one another.
 */
class ParamID
{
public:
	inline ParamID(AutomationType type = NullAutomation, uint32_t id=0) : _type(type), _id(id) {}
	
	/** Construct an ParamID from a string returned from ParamID::to_string
	 * (AutomationList automation-id property)
	 */
	ParamID(const std::string& str) : _type(NullAutomation), _id(0) {
		if (str == "gain") {
			_type = GainAutomation;
		} else if (str == "pan") {
			_type = PanAutomation;
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
		} else if (str.length() > 10 && str.substr(0, 10) == "parameter-") {
			_type = PluginAutomation;
			_id = atoi(str.c_str()+10);
			//PBD::info << "Parameter: " << str << " -> " << _id << endl;
		} else if (str.length() > 7 && str.substr(0, 7) == "midicc-") {
			_type = MidiCCAutomation;
			_id = atoi(str.c_str()+7);
			//PBD::info << "MIDI CC: " << str << " -> " << _id << endl;
		} else {
			PBD::warning << "Unknown ParamID '" << str << "'" << endmsg;
		}
	}

	inline AutomationType type() const { return _type; }
	inline uint32_t       id()   const { return _id; }

	inline bool operator==(const ParamID& id) const
		{ return (_type == id._type && _id == id._id); }
	
	/** Arbitrary but fixed ordering, so we're comparable (usable in std::map) */
	inline bool operator<(const ParamID& id) const {
		// FIXME: branch a performance problem?  #ifdef DEBUG?
		if (_type == NullAutomation)
			PBD::warning << "Uninitialized ParamID compared." << endmsg;
		return (_type < id._type || _id < id._id);
	}
	
	inline operator bool() const { return (_type != 0); }

	/** Unique string representation, suitable as an XML property value.
	 * e.g. <AutomationList automation-id="whatthisreturns">
	 */
	inline std::string to_string() const {
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
			return string_compose("midicc-%1", _id);
		} else {
			PBD::warning << "Uninitialized ParamID to_string() called." << endmsg;
			return "";
		}
	}

private:
	// default copy constructor is ok
	AutomationType _type;
	uint32_t       _id;
};


} // namespace ARDOUR

#endif // __ardour_param_id_h__

