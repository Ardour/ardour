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
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

static const char* state_node_name = "Channelmap";

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

ChanMapping::ChanMapping (const ChanMapping& other)
{
	const ChanMapping::Mappings& mp (other.mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			set (tm->first, i->first, i->second);
		}
	}
}

ChanMapping::ChanMapping (const XMLNode& node)
{
	XMLNodeConstIterator iter = node.children().begin();
	for ( ; iter != node.children().end(); ++iter) {
		if ((*iter)->name() == X_(state_node_name)) {
			DataType type(DataType::NIL);
			uint32_t from;
			uint32_t to;
			(*iter)->get_property("type", type);
			(*iter)->get_property("from", from);
			(*iter)->get_property("to", to);
			set(type, from, to);
		}
	}
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

uint32_t
ChanMapping::get_src(DataType t, uint32_t to, bool* valid) const
{
	Mappings::const_iterator tm = _mappings.find(t);
	if (tm == _mappings.end()) {
		if (valid) { *valid = false; }
		return -1;
	}
	for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
		if (i->second == to) {
			if (valid) { *valid = true; }
			return i->first;
		}
	}
	if (valid) { *valid = false; }
	return -1;
}



void
ChanMapping::set(DataType t, uint32_t from, uint32_t to)
{
	assert(t != DataType::NIL);
	Mappings::iterator tm = _mappings.find (t);
	if (tm == _mappings.end()) {
		tm = _mappings.insert(std::make_pair(t, TypeMapping())).first;
	}
	tm->second.insert(std::make_pair(from, to));
}

void
ChanMapping::unset(DataType t, uint32_t from)
{
	assert(t != DataType::NIL);
	Mappings::iterator tm = _mappings.find (t);
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
	if (tm != _mappings.end ()) {
		TypeMapping new_map;
		for (TypeMapping::iterator m = tm->second.begin(); m != tm->second.end(); ++m) {
			new_map.insert (make_pair (m->first + delta, m->second));
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

XMLNode*
ChanMapping::state(const std::string& name) const
{
	XMLNode* node = new XMLNode (name);
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			XMLNode* n = new XMLNode(X_(state_node_name));
			n->set_property("type", tm->first.to_string());
			n->set_property("from", i->first);
			n->set_property("to", i->second);
			node->add_child_nocopy(*n);
		}
	}
	return node;
}

bool
ChanMapping::is_subset (const ChanMapping& superset) const
{
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
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
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		uint32_t prev = UINT32_MAX;
		for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
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
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		for (TypeMapping::const_iterator i = tm->second.begin(); i != tm->second.end(); ++i) {
			if (i->first + offset.get (tm->first) != i->second) {
				return false;
			}
		}
	}
	return true;
}

uint32_t
ChanMapping::n_total () const
{
	// fast version of count().n_total();
	uint32_t rv = 0;
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		rv += tm->second.size ();
	}
	return rv;
}

ChanCount
ChanMapping::count () const
{
	ChanCount rv;
	const Mappings& mp (mappings());
	for (Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		rv.set (tm->first, tm->second.size ());
	}
	return rv;
}



} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanMapping& cm)
{
	const ARDOUR::ChanMapping::Mappings& mp (cm.mappings());
	for (ARDOUR::ChanMapping::Mappings::const_iterator tm = mp.begin(); tm != mp.end(); ++tm) {
		o << tm->first.to_string() << endl;
		for (ARDOUR::ChanMapping::TypeMapping::const_iterator i = tm->second.begin();
				i != tm->second.end(); ++i) {
			o << "\t" << i->first << " => " << i->second << endl;
		}
	}

	return o;
}
