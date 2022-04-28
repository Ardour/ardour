/*
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/graph_edges.h"
#include "ardour/route.h"
#include "ardour/track.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

void
GraphEdges::add (GraphVertex from, GraphVertex to, bool via_sends_only)
{
	insert (_from_to, from, to);
	insert (_to_from, to, from);

	EdgeMapWithSends::iterator i = find_in_from_to_with_sends (from, to);
	if (i != _from_to_with_sends.end ()) {
		i->second.second |= via_sends_only;
	} else {
		_from_to_with_sends.insert (make_pair (from, make_pair (to, via_sends_only)));
	}

	i = find_in_to_from_with_sends (to, from);
	if (i != _to_from_with_sends.end ()) {
		i->second.second |= via_sends_only;
	} else {
		_to_from_with_sends.insert (make_pair (to, make_pair (from, via_sends_only)));
	}
}

/** Find a from/to pair in the _from_to_with_sends map.
 *  @return iterator to the edge, or _from_to_with_sends.end().
 */
GraphEdges::EdgeMapWithSends::iterator
GraphEdges::find_in_from_to_with_sends (GraphVertex from, GraphVertex to)
{
	typedef EdgeMapWithSends::iterator Iter;
	pair<Iter, Iter> r = _from_to_with_sends.equal_range (from);
	for (Iter i = r.first; i != r.second; ++i) {
		if (i->second.first == to) {
			return i;
		}
	}
	return _from_to_with_sends.end ();
}

GraphEdges::EdgeMapWithSends::iterator
GraphEdges::find_in_to_from_with_sends (GraphVertex to, GraphVertex from)
{
	typedef EdgeMapWithSends::iterator Iter;
	pair<Iter, Iter> r = _to_from_with_sends.equal_range (to);
	for (Iter i = r.first; i != r.second; ++i) {
		if (i->second.first == from) {
			return i;
		}
	}
	return _to_from_with_sends.end ();
}

GraphEdges::EdgeMapWithSends::const_iterator
GraphEdges::find_recursively_in_from_to_with_sends (GraphVertex from, GraphVertex to) const
{
	typedef EdgeMapWithSends::const_iterator Iter;
	pair<Iter, Iter> r = _from_to_with_sends.equal_range (from);
	for (Iter i = r.first; i != r.second; ++i) {
		if (i->second.first == to) {
			return i;
		}
		Iter t = find_recursively_in_from_to_with_sends (i->second.first, to);
		if (t != _from_to_with_sends.end ()) {
			return t;
		}
	}

	return _from_to_with_sends.end ();
}

/** @param via_sends_only if non-0, filled in with true if the edge is a
 *  path via a send only.
 *  @return true if the given edge is present.
 */
bool
GraphEdges::has (GraphVertex from, GraphVertex to, bool* via_sends_only)
{
	EdgeMapWithSends::iterator i = find_in_from_to_with_sends (from, to);
	if (i == _from_to_with_sends.end ()) {
		return false;
	}

	if (via_sends_only) {
		*via_sends_only = i->second.second;
	}

	return true;
}

bool
GraphEdges::feeds (GraphVertex from, GraphVertex to) const
{
	EdgeMapWithSends::const_iterator i = find_recursively_in_from_to_with_sends (from, to);
	if (i == _from_to_with_sends.end ()) {
		return false;
	}
	return true;
}

set<GraphVertex>
GraphEdges::from (GraphVertex r) const
{
	EdgeMap::const_iterator i = _from_to.find (r);
	if (i == _from_to.end ()) {
		return set<GraphVertex> ();
	}

	return i->second;
}

set<GraphVertex>
GraphEdges::to (GraphVertex t, bool via_sends_only) const
{
	set<GraphVertex> rv;
	typedef EdgeMapWithSends::const_iterator Iter;
	pair<Iter, Iter> r = _to_from_with_sends.equal_range (t);
	for (Iter i = r.first; i != r.second; ++i) {
		if (via_sends_only) {
			if (!i->second.second) {
				continue;
			}
		}
		rv.insert (i->second.first);
		for (auto const& j: GraphEdges::to (i->second.first, i->second.second ? false : via_sends_only)) {
			rv.insert (j);
		}
	}
	return rv;
}

void
GraphEdges::remove (GraphVertex from, GraphVertex to)
{
	EdgeMap::iterator i = _from_to.find (from);
	assert (i != _from_to.end ());
	i->second.erase (to);
	if (i->second.empty ()) {
		_from_to.erase (i);
	}

	EdgeMap::iterator j = _to_from.find (to);
	assert (j != _to_from.end ());
	j->second.erase (from);
	if (j->second.empty ()) {
		_to_from.erase (j);
	}

	EdgeMapWithSends::iterator k = find_in_from_to_with_sends (from, to);
	assert (k != _from_to_with_sends.end ());
	_from_to_with_sends.erase (k);
}

/** @param to `To' route.
 *  @return true if there are no edges going to `to'.
 */

bool
GraphEdges::has_none_to (GraphVertex to) const
{
	return _to_from.find (to) == _to_from.end ();
}

bool
GraphEdges::empty () const
{
	assert (_from_to.empty () == _to_from.empty ());
	return _from_to.empty ();
}

void
GraphEdges::dump () const
{
	for (auto const& i : _from_to) {
		cout << "FROM: " << i.first->graph_node_name () << " ";
		for (auto const& j : i.second) {
			cout << j->graph_node_name () << " ";
		}
		cout << "\n";
	}

	for (auto const& i : _to_from) {
		cout << "TO: " << i.first->graph_node_name () << " ";
		for (auto const& j : i.second) {
			cout << j->graph_node_name () << " ";
		}
		cout << "\n";
	}
}

/** Insert an edge into one of the EdgeMaps */
void
GraphEdges::insert (EdgeMap& e, GraphVertex a, GraphVertex b)
{
	EdgeMap::iterator i = e.find (a);
	if (i != e.end ()) {
		i->second.insert (b);
	} else {
		set<GraphVertex> v;
		v.insert (b);
		e.insert (make_pair (a, v));
	}
}

struct RouteRecEnabledComparator
{
	bool operator () (GraphVertex n1, GraphVertex n2) const
	{
		boost::shared_ptr<Track> t1 (boost::dynamic_pointer_cast<Track>(n1));
		boost::shared_ptr<Track> t2 (boost::dynamic_pointer_cast<Track>(n2));
		boost::shared_ptr<Route> r1 (boost::dynamic_pointer_cast<Route>(n1));
		boost::shared_ptr<Route> r2 (boost::dynamic_pointer_cast<Route>(n2));

		PresentationInfo::order_t r1o = r1 ? r1->presentation_info().order() : 0;
		PresentationInfo::order_t r2o = r2 ? r2->presentation_info().order() : 0;

		if (!t1) {
			if (!t2) {
				/* makes no difference which is first, use presentation order */
				return r1o < r2o;
			} else {
				/* r1 is not a track, r2 is, run it early */
				return false;
			}
		}

		if (!t2) {
			/* we already tested !t1, so just use presentation order */
			return r1o < r2o;
		}

		if (t1->rec_enable_control()->get_value()) {
			if (t2->rec_enable_control()->get_value()) {
				/* both rec-enabled, just use signal order */
				return r1o < r2o;
			} else {
				/* t1 rec-enabled, t2 not rec-enabled, run t2 early */
				return false;
			}
		} else {
			if (t2->rec_enable_control()->get_value()) {
				/* t2 rec-enabled, t1 not rec-enabled, run t1 early */
				return true;
			} else {
				/* neither rec-enabled, use presentation order */
				return r1o < r2o;
			}
		}
	}
};

/** Perform a topological sort of a list of routes using a directed graph representing connections.
 *  @return Sorted list of routes, or 0 if the graph contains cycles (feedback loops).
 */
bool
ARDOUR::topological_sort (GraphNodeList& nodes, GraphEdges& edges)
{
	/* Collect the edges of the  graph.  Each of these edges
	 * is a pair of nodes, one of which directly feeds the other
	 * either by a port connection or by an internal send.
	 */

	for (auto const& i : nodes) {

		for (auto const& j : nodes) {

			bool via_sends_only = false;

			/* See if this *j feeds *i according to the current state of
			 * port connections and internal sends.
			 */
			if (j->direct_feeds_according_to_reality (i, &via_sends_only)) {
				/* add the edge to the graph (part #1) */
				edges.add (j, i, via_sends_only);
			}
		}
	}

	GraphNodeList queue;

	/* initial queue has routes that are not fed by anything */
	for (auto const& i : nodes) {
		if (edges.has_none_to (i)) {
			queue.push_back (i);
		}
	}

	/* Sort the initial queue so that non-rec-enabled routes are run first.
	 * This is so that routes can record things coming from other routes
	 * via external connections.
	 */
	queue.sort (RouteRecEnabledComparator ());

	/* Do the sort: algorithm is Kahn's from Wikipedia.
	 * `Topological sorting of large networks', Communications of the ACM 5(11):558-562.
	 */

	nodes.clear ();
	GraphEdges remaining_edges (edges);

	while (!queue.empty ()) {
		GraphVertex r = queue.front ();
		queue.pop_front ();
		nodes.push_back (r);
		set<GraphVertex> e = remaining_edges.from (r);
		for (set<GraphVertex>::iterator i = e.begin(); i != e.end(); ++i) {
			remaining_edges.remove (r, *i);
			if (remaining_edges.has_none_to (*i)) {
				queue.push_back (*i);
			}
		}
	}

	if (!remaining_edges.empty ()) {
		remaining_edges.dump ();
		/* There are cycles in the graph, so we can't do a topological sort */
		return false;
	}

	return true;
}
