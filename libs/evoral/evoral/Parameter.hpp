/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_PARAMETER_HPP
#define EVORAL_PARAMETER_HPP

#include <string>
#include <boost/format.hpp>

namespace Evoral {


/** ID of a [play|record|automate]able parameter.
 *
 * A parameter is defined by (type, id, channel).  Type is an integer which
 * can be used in any way by the application (e.g. cast to a custom enum,
 * map to/from a URI, etc).  ID is type specific (e.g. MIDI controller #).
 *
 * This class defines a < operator which is a strict weak ordering, so
 * Parameter may be stored in a std::set, used as a std::map key, etc.
 */
class Parameter
{
public:
	Parameter(uint32_t type, uint32_t id, int8_t channel=0,
			double min=0.0f, double max=0.0f, double def=0.0f)
		: _type(type), _id(id), _channel(channel), _min(min), _max(max), _normal(def)
	{}
	
	//Parameter(const std::string& str);

	inline uint32_t type()    const { return _type; }
	inline uint32_t id()      const { return _id; }
	inline uint8_t  channel() const { return _channel; }

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
	 * See: http://www.sgi.com/tech/stl/StrictWeakOrdering.html
	 * Sort Parameters first according to type then to id and lastly to channel.
	 *  
	 * Proof:
	 * <ol>
	 * <li>Irreflexivity: f(x, x) is false because of the irreflexivity of \c < in each branch.</li>
	 * <li>Antisymmetry: given x != y, f(x, y) implies !f(y, x) because of the same 
	 *     property of \c < in each branch and the symmetry of operator==. </li>
	 * <li>Transitivity: let f(x, y) and f(y, z) be true.
	 *    We prove by contradiction, assuming the contrary (f(x, z) is false).
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
	 *      Therefore f(x, z) is true (transitivity)
	 *    </ol> 
	 * </li>
	 * </ol>
	 */
	inline bool operator<(const Parameter& id) const {
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
	
	virtual std::string symbol() const {
		return (boost::format("%1%_c%2%_n%3%\n") % _type % _channel % _id).str();
	}
	
	inline void set_range(double min, double max, double normal) {
		_min = min;
		_max = max;
		_normal = normal;
	}
	
	inline const double min()    const { return _min; }
	inline const double max()    const { return _max; }
	inline const double normal() const { return _normal; }

protected:
	// Default copy constructor is ok
	
	// ID (used in comparison)
	uint32_t _type;
	uint32_t _id;
	uint8_t  _channel;

	// Metadata (not used in comparison)
	double _min;
	double _max;
	double _normal;
};


} // namespace Evoral

#endif // EVORAL_PARAMETER_HPP

