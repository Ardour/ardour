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

struct LIBTEMPORAL_API TimeDomainSwapper : public virtual PBD::Destructible {
	virtual ~TimeDomainSwapper() {}
	virtual void swap_domain (Temporal::TimeDomain from, Temporal::TimeDomain to) = 0;
};


struct LIBTEMPORAL_API TimeDomainCommand : public PBD::Command {
  public:
	TimeDomainCommand (TimeDomain f, TimeDomain t) : from (f), to (t) {}
	void add (TimeDomainSwapper&);

	void operator() ();
	void undo ();

  private:
	TimeDomain from;
	TimeDomain to;

	typedef std::set<TimeDomainSwapper*> Swappers;
	Swappers swappers;
	PBD::ScopedConnectionList tds_connections;

	void going_away (TimeDomainSwapper*);

};

class LIBTEMPORAL_API DomainSwapInformation {
   public:
	static DomainSwapInformation* start (TimeDomain prev);

	~DomainSwapInformation ();

	void add (timecnt_t& t) { counts.push_back (&t); }
	void add (timepos_t& p) { positions.push_back (&p); }
	void add (TimeDomainSwapper& tt) { time_things.push_back (&tt); }
	void clear ();

   private:
	DomainSwapInformation (TimeDomain prev) : previous (prev) {}

	std::vector<timecnt_t*> counts;
	std::vector<timepos_t*> positions;
	std::vector<TimeDomainSwapper*> time_things;
	TimeDomain previous;

	void undo ();
};

extern LIBTEMPORAL_API DomainSwapInformation* domain_swap;

}

#endif /* __tmeporal_domain_swap_h__ */
