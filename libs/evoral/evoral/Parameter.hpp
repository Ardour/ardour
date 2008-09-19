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
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <iostream>

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
	Parameter(uint32_t type, uint8_t channel, uint32_t id=0)
		: _type(type), _id(id), _channel(channel)
	{}
    
	Parameter(const std::string& str) {
		int channel;
		if (sscanf(str.c_str(), "%d_c%d_n%d", &_type, &channel, &_id) == 3) {
			if (channel >= 0 && channel <= 127) {
				_channel = channel;
			} else {
				std::cerr << "WARNING: Channel out of range: " << channel << std::endl;
			}
		}
		std::cerr << "WARNING: Unable to create parameter from string: " << str << std::endl;
	}

	virtual ~Parameter() {}
    
	inline uint32_t type()    const { return _type; }
	inline uint32_t id()      const { return _id; }
	inline uint8_t  channel() const { return _channel; }

	/** Equivalence operator
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
		return (boost::format("%1%_c%2%_n%3%") % _type % (int)_channel % _id).str();
	}

	/** Not used in indentity/comparison */
	struct Metadata {
		Metadata(double low=0.0, double high=1.0, double mid=0.0)
			: min(low), max(high), normal(mid)
		{}
		double min;
		double max;
		double normal;
	};
	
	inline static void set_range(uint32_t type, double min, double max, double normal) {
		_type_metadata[type] = Metadata(min, max, normal);
	}
	
	inline void set_range(double min, double max, double normal) {
		_metadata = boost::shared_ptr<Metadata>(new Metadata(min, max, normal));
	}

	inline Metadata& metadata() const {
		if (_metadata)
			return *_metadata.get();
		else
			return _type_metadata[_type];
	}

	inline const double min()    const { return metadata().min; }
	inline const double max()    const { return metadata().max; }
	inline const double normal() const { return metadata().normal; }

protected:
	// Default copy constructor is ok
	
	// ID (used in comparison)
	uint32_t _type;
	uint32_t _id;
	uint8_t  _channel;
	
	boost::shared_ptr<Metadata> _metadata;

	typedef std::map<uint32_t, Metadata> TypeMetadata;
	static TypeMetadata _type_metadata;
};


} // namespace Evoral

#endif // EVORAL_PARAMETER_HPP

