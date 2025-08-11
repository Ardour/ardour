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

namespace Temporal {

class ScopedTempoMapOwner
{
 public:
	ScopedTempoMapOwner () : local_tempo_map_depth (0) {}
	virtual ~ScopedTempoMapOwner () {}

	void start_local_tempo_map (std::shared_ptr<Temporal::TempoMap> map) {
		_local_tempo_map = map;
		in ();
		DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: starting local tempo scope\n", scope_name()));
	}

	void end_local_tempo_map () {
		DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: ending local tempo scope\n", scope_name()));
		local_tempo_map_depth = 1; /* force exit in out() */
		out ();
		_local_tempo_map.reset ();
	}

	uint64_t depth() const { return local_tempo_map_depth; }

	virtual std::string scope_name() const = 0;

  protected:
	mutable std::shared_ptr<Temporal::TempoMap> _local_tempo_map;
	mutable uint64_t local_tempo_map_depth;

  private:
	friend class TempoMapScope;

	void in () const {
		if (_local_tempo_map && local_tempo_map_depth++ == 0 ) {
			DEBUG_TRACE (PBD::DEBUG::ScopedTempoMap, string_compose ("%1: in to local tempo  %2\n", scope_name(), local_tempo_map_depth));
			Temporal::TempoMap::set (_local_tempo_map);
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
