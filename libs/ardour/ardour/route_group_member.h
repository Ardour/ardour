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

#ifndef __libardour_route_group_member_h__
#define __libardour_route_group_member_h__


namespace ARDOUR  {

class RouteGroup;

class RouteGroupMember 
{
  public:
	RouteGroupMember () : _route_group (0) {}
	virtual ~RouteGroupMember() {}

	RouteGroup* route_group () const { return _route_group; }

	boost::signals2::signal<void()> route_group_changed;

  protected:
	RouteGroup* _route_group;

  private:
	friend class RouteGroup;

	void join_route_group (RouteGroup*);
	void leave_route_group ();
};

}

#endif /* __libardour_route_group_member_h__ */
