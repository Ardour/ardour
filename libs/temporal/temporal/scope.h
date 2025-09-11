/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <cstdint>
#include <memory>

#include "pbd/compose.h"

#include "temporal/debug.h"
#include "temporal/tempo.h"
#include "temporal/visibility.h"

namespace Temporal {

class TempoMap;

/* This object aand the use of the EC_LOCAL_TEMPO_MAP_SCOPE allows derived
 * classes to have a "local" (i.e. non-global) tempo map in use. This is
 * intended for custom editing contexts where all conversion between
 * sample/pixel and beat time should use a local tempo map, rather than the
 * global one.
 *
 * The downside is a bit ugly: every method of a derived class should have a
 * call to EC_LOCAL_TEMPO_MAP_SCOPE before anything else. However, in C++ there
 * is no other way to accomplish this, since there is no method-based code
 * injection. This macro uses an RAII technique (via TempoMapScope) to call the
 * ::in() and ::out() methods of the ScopedTempoMapOwner on entry and exist
 * from the method scope.
 *
 * A derived class will call start_local_tempo_map() to provide the map it
 * should be using, and end_local_tempo_map() when for whatever reason that map
 * is no longer relevant.
 *
 * start_local_tempo_map() will set the local_tempo_map (shared) ptr, which
 * gives us an indication as we enter and leave the scope of class methods that
 * there is a map which should be in use. There is also a depth counter so that
 * we only set the thread-local tempo map pointer when we transition from
 * depth==0 to depth==1. See notes in the method definition for some important
 * caveats.
 *
 * end_local_tempo_map() is called when the object's methods should no longer
 * use the previously provided local tempo map.
 *
 * The cost of this is low (but not zero, obviously):
 *
 *       - extra pointer on the stack (the scope member of TempoMapScope)
 *       - two conditionals, the first of which will frequently fail
 *       - writes to the thread-local pointer whenever the method dept
 *       - transitions between 0 and 1 (in either direction).
 */

class LIBTEMPORAL_API ScopedTempoMapOwner
{
 public:
	ScopedTempoMapOwner () : local_tempo_map_depth (0) {}
	virtual ~ScopedTempoMapOwner () {}

	void start_local_tempo_map (std::shared_ptr<Temporal::TempoMap> map);
	void end_local_tempo_map ();

	uint64_t depth() const { return local_tempo_map_depth; }

	virtual std::string scope_name() const = 0;

  protected:
	mutable std::shared_ptr<Temporal::TempoMap> _local_tempo_map;
	mutable uint64_t local_tempo_map_depth;

  private:
	friend struct TempoMapScope;

	void in () const {
		if (_local_tempo_map && local_tempo_map_depth++ == 0 ) {
			DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: in to local tempo  %2, TMAP set\n", scope_name(), local_tempo_map_depth));
			Temporal::TempoMap::set (_local_tempo_map);
		} else {
			DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: in to local tempo  %2, no tm set\n", scope_name(), local_tempo_map_depth));
		}
	}

	void out () const {
		DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: out with local tempo  %2\n", scope_name(), local_tempo_map_depth));
		if (local_tempo_map_depth && --local_tempo_map_depth == 0) {
			DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: done with local tempo, depth now %2\n", scope_name(), local_tempo_map_depth));
			Temporal::TempoMap::fetch (); /* get current global map into thread-local pointer */
		}
	}

};


struct TempoMapScope {
	TempoMapScope (ScopedTempoMapOwner const & sco)
		: scope (sco)
	{
		scope.in ();
	}

	~TempoMapScope () {
		scope.out ();
	}

	ScopedTempoMapOwner const & scope;
};

} // namespace 

#define EC_LOCAL_TEMPO_SCOPE Temporal::TempoMapScope __tms (*this);
#define EC_LOCAL_TEMPO_SCOPE_ARG(arg) Temporal::TempoMapScope __tms (arg);
