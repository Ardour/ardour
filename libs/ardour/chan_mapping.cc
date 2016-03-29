/*
    Copyright (C) 2009 Paul Davis
    Author: David Robillard

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

    $Id: insert.cc 712 2006-07-28 01:08:57Z drobilla $
*/

#include <stdint.h>
#include <iostream>
#include "ardour/chan_mapping.h"

using namespace std;

namespace ARDOUR {

ChanMapping::ChanMapping(ChanCount identity)
{
	if (identity == ChanCount::INFINITE) {
		return;
	}

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (size_t i = 0; i < identity.get(*t); ++i) {
			set(*t, i, i);
		}
	}
}

ChanMapping::ChanMapping (const ChanMapping& other )
	: _mappings (other._mappings)
{
}

uint32_t
ChanMapping::get(DataType t, uint32_t from, bool* valid) const
{
	Mappings::const_iterator tm = _mappings.find(t);
	if (tm == _mappings.end()) {
		if (valid) { *valid = false; }
		return -1;
	}
	TypeMapping::const_iterator m = tm->second.find(from);
	if (m == tm->second.end()) {
		if (valid) { *valid = false; }
		return -1;
	}
	if (valid) { *valid = true; }
	return m->second;
}

void
ChanMapping::set(DataType t, uint32_t from, uint32_t to)
{
	assert(t != DataType::NIL);
	Mappings::iterator tm = _mappings.find(t);
	if (tm == _mappings.end()) {
		tm = _mappings.insert(std::make_pair(t, TypeMapping())).first;
	}
	tm->second.insert(std::make_pair(from, to));
}

void
ChanMapping::unset(DataType t, uint32_t from)
{
	assert(t != DataType::NIL);
	Mappings::iterator tm = _mappings.find(t);
	if (tm == _mappings.end()) {
		return;
	}
	tm->second.erase(from);
}

/** Offset the 'from' field of every mapping for type @a t by @a delta */
void
ChanMapping::offset_from(DataType t, int32_t delta)
{
	Mappings::iterator tm = _mappings.find(t);
	if (tm != _mappings.end()) {
		TypeMapping new_map;
		for (TypeMapping::iterator m = tm->second.begin(); m != tm->second.end(); ++m) {
			new_map.insert(make_pair(m->first + delta, m->second));
		}
		tm->second = new_map;
	}
}

/** Offset the 'to' field of every mapping for type @a t by @a delta */
void
ChanMapping::offset_to(DataType t, int32_t delta)
{
	Mappings::iterator tm = _mappings.find(t);
	if (tm != _mappings.end()) {
		for (TypeMapping::iterator m = tm->second.begin(); m != tm->second.end(); ++m) {
			m->second += delta;
		}
	}
}

bool
ChanMapping::is_subset (const ChanMapping& superset) const
{
	for (ARDOUR::ChanMapping::Mappings::const_iterator tm = mappings().begin(); tm != mappings().end(); ++tm) {
		for (ARDOUR::ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			bool valid;
			if (i->second != superset.get (tm->first, i->first, &valid)) {
				return false;
			}
			if (!valid) {
				return false;
			}
		}
	}
	return true;
}

bool
ChanMapping::is_monotonic () const
{
	for (ARDOUR::ChanMapping::Mappings::const_iterator tm = mappings().begin(); tm != mappings().end(); ++tm) {
		uint32_t prev = UINT32_MAX;
		for (ARDOUR::ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			// set keys are strictly weak ordered
			if (i->first < i->second || i->second == prev) {
				return false;
			}
			prev = i->second;
		}
	}
	return true;
}

bool
ChanMapping::is_identity (ChanCount offset) const
{
	for (ARDOUR::ChanMapping::Mappings::const_iterator tm = mappings().begin(); tm != mappings().end(); ++tm) {
		for (ARDOUR::ChanMapping::TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			if (i->first + offset.get (tm->first) != i->second) {
				return false;
			}
		}
	}
	return true;
}

} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanMapping& cm)
{
	for (ARDOUR::ChanMapping::Mappings::const_iterator tm = cm.mappings().begin();
			tm != cm.mappings().end(); ++tm) {
		o << tm->first.to_string() << endl;
		for (ARDOUR::ChanMapping::TypeMapping::const_iterator i = tm->second.begin();
				i != tm->second.end(); ++i) {
			o << "\t" << i->first << " => " << i->second << endl;
		}
	}

	return o;
}

