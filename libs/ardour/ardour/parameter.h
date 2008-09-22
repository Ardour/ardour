/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard
    
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

#ifndef __ardour_parameter_h__
#define __ardour_parameter_h__

#include <string>
#include <pbd/compose.h>
#include <pbd/error.h>
#include <ardour/types.h>
#include <evoral/Parameter.hpp>
#include <evoral/MIDIParameters.hpp>

namespace ARDOUR {

/** ID of an automatable parameter.
 *
 * A given automatable object has a number of automatable parameters.  This is
 * the unique ID for those parameters.  Anything automatable (AutomationList,
 * Curve) must have unique Parameter ID with respect to it's Automatable parent.
 *
 * These are fast to compare, but passing a (const) reference around is
 * probably more efficient than copying because the Parameter contains
 * metadata not used for comparison.
 *
 * See evoral/Parameter.hpp for precise definition.
 */
class Parameter : public Evoral::Parameter
{
public:
	Parameter(AutomationType type = NullAutomation, uint32_t id=0, uint8_t channel=0)
		: Evoral::Parameter((uint32_t)type, id, channel)
	{
		init_metadata(type);
	}
	
	Parameter(const Evoral::Parameter& copy)
		: Evoral::Parameter(copy)
	{
	}
	
	static void init_metadata(AutomationType type) {
		double min    = 0.0f;
		double max    = 1.0f;
		double normal = 0.0f;
		switch(type) {
		case NullAutomation:
		case GainAutomation:
			max = 2.0f;
			normal = 1.0f;
			break;
		case PanAutomation:
			normal = 0.5f;
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
		}
		set_range(type, min, max, normal);
	}
	
	Parameter(const std::string& str);

	inline AutomationType type() const { return (AutomationType)_type; }

	std::string symbol() const;

	inline bool is_integer() const {
		return (_type >= MidiCCAutomation && _type <= MidiChannelPressureAutomation);
	}

	inline operator Parameter() { return (Parameter)*this; }
};


} // namespace ARDOUR

#endif // __ardour_parameter_h__

