/*
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <map>
#include <set>

namespace ARDOUR {

/** A list of edges for a directed acyclic graph for routes */	
class DAGEdges
{
public:
	typedef std::map<boost::shared_ptr<Route>, std::set<boost::shared_ptr<Route> > > EdgeMap;
	
	void add (boost::shared_ptr<Route> from, boost::shared_ptr<Route> to);
	std::set<boost::shared_ptr<Route> > from (boost::shared_ptr<Route> r) const;
	void remove (boost::shared_ptr<Route> from, boost::shared_ptr<Route> to);
	bool has_none_to (boost::shared_ptr<Route> to) const;
	bool empty () const;
	void dump () const;
	
private:
	void insert (EdgeMap& e, boost::shared_ptr<Route> a, boost::shared_ptr<Route> b);

	/* Keep a map in both directions to speed lookups */

	/** map of edges with from as `first' and to as `second' */
	EdgeMap _from_to;
	/** map of the same edges with to as `first' and from as `second' */
	EdgeMap _to_from;
};

boost::shared_ptr<RouteList> topographical_sort (
	boost::shared_ptr<RouteList>,
	DAGEdges
	);

}

