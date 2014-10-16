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

#ifndef __ardour_route_graph_h__
#define __ardour_route_graph_h__

#include <map>
#include <set>

namespace ARDOUR {

typedef boost::shared_ptr<Route> GraphVertex;

/** A list of edges for a directed graph for routes.
 *
 *  It keeps the same data in a few different ways, with add() adding edges
 *  to all different representations, remove() removing similarly, and the
 *  lookup method using whichever representation is most efficient for
 *  that particular lookup.
 *
 *  This may be a premature optimisation...
 */
class LIBARDOUR_API GraphEdges
{
public:
	typedef std::map<GraphVertex, std::set<GraphVertex> > EdgeMap;
	
	void add (GraphVertex from, GraphVertex to, bool via_sends_only);
	bool has (GraphVertex from, GraphVertex to, bool* via_sends_only);
	std::set<GraphVertex> from (GraphVertex r) const;
	void remove (GraphVertex from, GraphVertex to);
	bool has_none_to (GraphVertex to) const;
	bool empty () const;
	void dump () const;
	
private:
	void insert (EdgeMap& e, GraphVertex a, GraphVertex b);
	
	typedef std::multimap<GraphVertex, std::pair<GraphVertex, bool> > EdgeMapWithSends;
	
	EdgeMapWithSends::iterator find_in_from_to_with_sends (GraphVertex, GraphVertex);

	/** map of edges with from as `first' and to as `second' */
	EdgeMap _from_to;
	/** map of the same edges with to as `first' and from as `second' */
	EdgeMap _to_from;
	/** map of edges with via-sends information; first part of the map is
	    the `from' vertex, second is the `to' vertex and a flag which is
	    true if the edge is via a send only.
	*/
	EdgeMapWithSends _from_to_with_sends;
};

boost::shared_ptr<RouteList> topological_sort (
	boost::shared_ptr<RouteList>,
	GraphEdges
	);

}

#endif
