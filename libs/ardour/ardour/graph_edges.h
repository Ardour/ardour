/*
 * Copyright (C) 2015-2022 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_route_graph_h__
#define __ardour_route_graph_h__

#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {
class GraphNode;

typedef boost::shared_ptr<GraphNode> GraphVertex;

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
	void add (GraphVertex from, GraphVertex to, bool via_sends_only);
	void remove (GraphVertex from, GraphVertex to);

	bool has (GraphVertex from, GraphVertex to, bool* via_sends_only);
	bool feeds (GraphVertex from, GraphVertex to) const;
	/** @return the vertices that are directly fed from `r' */
	std::set<GraphVertex> from (GraphVertex r) const;
	/** @return all nodes that feed `r' (`r` is fed-by rv) */
	std::set<GraphVertex> to (GraphVertex r, bool via_sends_only = false) const;
	bool has_none_to (GraphVertex to) const;
	bool empty () const;
	void dump () const;

private:
	typedef std::map<GraphVertex, std::set<GraphVertex> > EdgeMap;
	typedef std::multimap<GraphVertex, std::pair<GraphVertex, bool> > EdgeMapWithSends;

	void insert (EdgeMap& e, GraphVertex a, GraphVertex b);

	EdgeMapWithSends::iterator find_in_from_to_with_sends (GraphVertex, GraphVertex);
	EdgeMapWithSends::iterator find_in_to_from_with_sends (GraphVertex, GraphVertex);

	EdgeMapWithSends::const_iterator find_recursively_in_from_to_with_sends (GraphVertex, GraphVertex) const;

	/** map of edges with from as `first' and to as `second' */
	EdgeMap _from_to;
	/** map of the same edges with to as `first' and from as `second' */
	EdgeMap _to_from;
	/** map of edges with via-sends information; first part of the map is
	 *  the `from' vertex, second is the `to' vertex and a flag which is
	 *  true if the edge is via a send only.
	 */
	EdgeMapWithSends _from_to_with_sends;
	EdgeMapWithSends _to_from_with_sends;
};

bool topological_sort (GraphNodeList&, GraphEdges&);

}

#endif
