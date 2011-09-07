/*
    Copyright (C) 2009 Paul Davis

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


#include "ardour/route_group.h"
#include "ardour/route_group_member.h"

using namespace ARDOUR;

void
RouteGroupMember::join_route_group (RouteGroup *rg)
{
	if (rg == _route_group) {
		return;
	}

	_route_group = rg;
	route_group_changed (rg); /* EMIT SIGNAL */
}

void
RouteGroupMember::leave_route_group ()
{
	RouteGroup* rg = _route_group;
	_route_group = 0;
	route_group_changed (rg); /* EMIT SIGNAL */
}

