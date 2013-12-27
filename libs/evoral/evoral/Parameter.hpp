/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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
#include <stdint.h>
#include <boost/shared_ptr.hpp>

#include "evoral/visibility.h"

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
class LIBEVORAL_API Parameter
{
public:
	Parameter(uint32_t type, uint8_t channel=0, uint32_t id=0)
		: _type(type), _id(id), _channel(channel)
	{}

	virtual ~Parameter() {}

	inline uint32_t type()    const { return _type; }
	inline uint8_t  channel() const { return _channel; }
	inline uint32_t id()      const { return _id; }

	/** Equivalence operator
	 * It is obvious from the definition that this operator
	 * is transitive, as required by stict weak ordering
	 * (see: http://www.sgi.com/tech/stl/StrictWeakOrdering.html)
	 */
	inline bool operator==(const Parameter& id) const {
		return (_type == id._type && _channel == id._channel && _id == id._id );
	}

	/** Strict weak ordering
	 * See: http://www.sgi.com/tech/stl/StrictWeakOrdering.html
	 * Sort Parameters first according to type then to channel and lastly to ID.
	 */
	inline bool operator<(const Parameter& other) const {
		if (_type < other._type) {
			return true;
		} else if (_type == other._type && _channel < other._channel) {
			return true;
		} else if (_type == other._type && _channel == other._channel && _id < other._id ) {
			return true;
		}

		return false;
	}

	inline operator bool() const { return (_type != 0); }

	/** Not used in indentity/comparison */
	struct Metadata {
		Metadata(double low=0.0, double high=1.0, double mid=0.0, bool tog=false)
			: min(low), max(high), normal(mid), toggled(tog)
		{}
		double min;
		double max;
		double normal;
		bool   toggled;
	};

	inline static void set_range(uint32_t type, double min, double max, double normal, bool toggled) {
		_type_metadata[type] = Metadata(min, max, normal, toggled);
	}

	inline void set_range(double min, double max, double normal, bool toggled) {
		_metadata = boost::shared_ptr<Metadata>(new Metadata(min, max, normal, toggled));
	}

	inline Metadata& metadata() const {
		if (_metadata)
			return *_metadata.get();
		else
			return _type_metadata[_type];
	}

	inline double min()     const { return metadata().min; }
	inline double max()     const { return metadata().max; }
	inline double normal()  const { return metadata().normal; }
	inline double toggled() const { return metadata().toggled; }

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

