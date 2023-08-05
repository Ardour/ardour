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

namespace Temporal {

struct LIBTEMPORAL_API TimeDomainSwapper : public virtual PBD::Destructible {
	virtual ~TimeDomainSwapper() {}
	virtual void swap_domain (Temporal::TimeDomain from, Temporal::TimeDomain to) = 0;
};


struct LIBTEMPORAL_API TimeDomainCommand : public Command {
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

}

#endif /* __tmeporal_domain_swap_h__ */
