/*
    Copyright (C) 2009 Paul Davis 
	Author: Dave Robillard

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

#ifndef __ardour_chan_mapping_h__
#define __ardour_chan_mapping_h__

#include <map>
#include <cassert>
#include <ostream>
#include <utility>

#include "ardour/data_type.h"

namespace ARDOUR {


/** A mapping from one set of channels to another
 * (e.g. how to 'connect' two BufferSets).
 */
class ChanMapping {
public:
	ChanMapping() {}
	ChanMapping(ChanCount identity) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			for (size_t i = 0; i <= identity.get(*t); ++i)
				set(*t, i, i);
		}
	}

	uint32_t get(DataType t, uint32_t from) {
		Mappings::iterator tm = _mappings.find(t);
		assert(tm != _mappings.end());
		TypeMapping::iterator m = tm->second.find(from);
		assert(m != tm->second.end());
		return m->second;
	}
	
	void set(DataType t, uint32_t from, uint32_t to) {
		Mappings::iterator tm = _mappings.find(t);
		if (tm == _mappings.end()) {
			tm = _mappings.insert(std::make_pair(t, TypeMapping())).first;
		}
		tm->second.insert(std::make_pair(from, to));
	}

	/** Increase the 'to' field of every mapping for type @a t by @a delta */
	void offset(DataType t, uint32_t delta) {
		Mappings::iterator tm = _mappings.find(t);
		if (tm != _mappings.end()) {
			for (TypeMapping::iterator m = tm->second.begin(); m != tm->second.end(); ++m) {
				m->second += delta;
			}
		}
	}

private:
	typedef std::map<uint32_t, uint32_t>    TypeMapping;
	typedef std::map<DataType, TypeMapping> Mappings;
	
	Mappings _mappings;
};

} // namespace ARDOUR

#endif // __ardour_chan_mapping_h__

