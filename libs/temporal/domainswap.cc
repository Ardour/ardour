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

#include "temporal/domainswap.h"

using namespace Temporal;

void
TimeDomainCommand::add (TimeDomainSwapper& tds)
{
	tds.DropReferences.connect_same_thread (tds_connections, boost::bind (&TimeDomainCommand::going_away, this, &tds));
	swappers.insert (&tds);
}

void
TimeDomainCommand::going_away (TimeDomainSwapper* tds)
{
	Swappers::iterator i = swappers.find (tds);

	if (i != swappers.end()) {
		swappers.erase (i);
	}
}

void
TimeDomainCommand::operator() ()
{
	for (auto & swapper : swappers) {
		swapper->swap_domain (from, to);
	}
}

void
TimeDomainCommand::undo ()
{
	for (auto & swapper : swappers) {
		swapper->swap_domain (to, from);
	}
}
