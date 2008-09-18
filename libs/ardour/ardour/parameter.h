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
class Parameter
{
public:
	Parameter(AutomationType type = NullAutomation, uint32_t id=0, uint8_t channel=0)
		: _type(type), _id(id), _channel(channel)
	{}
	
	Parameter(const std::string& str);

	inline AutomationType type()    const { return _type; }
	inline uint32_t       id()      const { return _id; }
	inline uint8_t        channel() const { return _channel; }

	/**
	 * Equivalence operator
	 * It is obvious from the definition that this operator
	 * is transitive, as required by stict weak ordering
	 * (see: http://www.sgi.com/tech/stl/StrictWeakOrdering.html)
	 */
	inline bool operator==(const Parameter& id) const {
		return (_type == id._type && _id == id._id && _channel == id._channel);
	}
	
	/** Strict weak ordering 
	 * (see: http://www.sgi.com/tech/stl/StrictWeakOrdering.html)  
	 * This is necessary so that std::set works): 
	 * Sort Parameters first according to type then to id and lastly to channel.
	 *  
	 * Proof:
	 * <ol>
	 * <li>Irreflexivity: f(x, x) is false because of the irreflexivity of \c < in each branch.</li>
	 * 
	 * <li>Antisymmetry: given x != y, f(x, y) implies !f(y, x) because of the same 
	 *     property of \c < in each branch and the symmetry of operator==. </li>
	 * 
	 * <li>Transitivity: let f(x, y) and f(y, z) be true. We prove by assuming the contrary,
	 *     that f(x, z) does not hold. 
	 *    That would imply exactly one of the following:
	 * 	  <ol>
	 *      <li> x == z which contradicts the assumption f(x, y) and f(y, x)
	 *                 because of antisymmetry.
	 *      </li>
	 *      <li> f(z, x) is true. That would imply that one of the ivars (we call it i) 
	 *           of x is greater than the same ivar in z while all "previous" ivars
	 *           are equal. That would imply that also in y all those "previous"
	 *           ivars are equal and because if x.i > z.i it is impossible
	 *           that there is an y that satisfies x.i < y.i < z.i at the same
	 *           time which contradicts the assumption.
	 *      </li>
	 *    </ol> 
	 * </li>
	 * </ol>
	 */
	inline bool operator<(const Parameter& id) const {
#ifndef NDEBUG
		if (_type == NullAutomation)
			PBD::warning << "Uninitialized Parameter compared." << endmsg;
#endif
		if (_type < id._type) {
			return true;
		} else if (_type == id._type && _id < id._id) {
			return true;
		} else if (_id == id._id && _channel < id._channel) {
			return true;
		}
		
		return false;
	}
	
	inline operator bool() const { return (_type != 0); }

	std::string to_string() const;

	/* The below properties are only used for CC right now, but unchanging properties
	 * of parameters (rather than changing parameters of automation lists themselves)
	 * should be moved here */

	inline double min() const {
		switch(_type) {
		case MidiCCAutomation:
		case MidiPgmChangeAutomation:
		case MidiPitchBenderAutomation:
		case MidiChannelAftertouchAutomation:
			return 0.0;
			
		default:
			return DBL_MIN;
		}
	}
	
	inline double max() const {
		switch(_type) {
		case MidiCCAutomation:
		case MidiPgmChangeAutomation:
		case MidiChannelAftertouchAutomation:
			return 127.0;
		case MidiPitchBenderAutomation:
			return 16383.0;
			
		default:
			return DBL_MAX;
		}
	}

	inline bool is_integer() const {
		return (_type >= MidiCCAutomation && _type <= MidiChannelAftertouchAutomation);
	}

private:
	// default copy constructor is ok
	AutomationType _type;
	uint32_t       _id;
	uint8_t        _channel;
};


} // namespace ARDOUR

#endif // __ardour_parameter_h__

