/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __temporal_domain_swap_h__
#define __temporal_domain_swap_h__

#include <set>

#include "pbd/command.h"
#include "pbd/destructible.h"
#include "pbd/signals.h"

#include "temporal/types.h"
#include "temporal/timeline.h"

namespace Temporal {

typedef std::map<timepos_t*,timepos_t> TimeDomainPosChanges;
typedef std::map<timecnt_t*,timecnt_t> TimeDomainCntChanges;

struct LIBTEMPORAL_API DomainBounceInfo;

struct LIBTEMPORAL_API TimeDomainSwapper : public virtual PBD::Destructible {
  public:
	virtual ~TimeDomainSwapper() {}
	virtual void start_domain_bounce (DomainBounceInfo&) = 0;
	virtual void finish_domain_bounce (DomainBounceInfo&) = 0;
};

/* A DomainBounceInfo functions in two roles:
 *
 * 1. as part of an UndoTransaction reflecting actions taken by a user that
 * modified time domains of one or more objects.
 *
 * 2. as a standalone object used during temporary domain swaps that records
 * (perhaps opaquely) what was changed and provides a way to revert it.
 */

struct LIBTEMPORAL_API DomainBounceInfo
{
	DomainBounceInfo (TimeDomain f, TimeDomain t, bool m = false)
		: from (f)
		, to (t)
		, move_markers (m)
	{}

	const TimeDomain from;
	const TimeDomain to;

	TimeDomainPosChanges positions;
	TimeDomainCntChanges counts;

	bool move_markers;
};

}

#endif /* __tmeporal_domain_swap_h__ */
