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
		init(type);
	}
	
	Parameter(AutomationType type, double min, double max, double normal)
		: Evoral::Parameter((uint32_t)type, 0, 0, min, max, normal)
	{}
	
	Parameter(const Evoral::Parameter& copy)
		: Evoral::Parameter(copy)
	{
		init((AutomationType)_type);
	}
	
	void init(AutomationType type) {
		_normal = 0.0f;
		switch(type) {
		case NullAutomation:
		case GainAutomation:
			_min = 0.0f;
			_max = 2.0f;
			_normal = 1.0f;
			break;
		case PanAutomation:
			_min = 0.0f;
			_max = 1.0f;
			_normal = 0.5f;
		case PluginAutomation:
		case SoloAutomation:
		case MuteAutomation:
		case FadeInAutomation:
		case FadeOutAutomation:
		case EnvelopeAutomation:
			_min = 0.0f;
			_max = 2.0f;
			_normal = 1.0f;
		case MidiCCAutomation:
			Evoral::MIDI::ContinuousController::set_range(*this); break;
		case MidiPgmChangeAutomation:
			Evoral::MIDI::ProgramChange::set_range(*this); break;
		case MidiPitchBenderAutomation:
			Evoral::MIDI::PitchBender::set_range(*this); break;
		case MidiChannelAftertouchAutomation:
			Evoral::MIDI::ChannelAftertouch::set_range(*this); break;
		}
	}
	
	Parameter(const std::string& str);

	inline AutomationType type() const { return (AutomationType)_type; }

	std::string symbol() const;

	inline bool is_integer() const {
		return (_type >= MidiCCAutomation && _type <= MidiChannelAftertouchAutomation);
	}

	inline operator Parameter() { return (Parameter)*this; }
};


} // namespace ARDOUR

#endif // __ardour_parameter_h__

