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

#include "ardour/route.h"
#include "ardour/route_graph.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

void
GraphEdges::add (GraphVertex from, GraphVertex to, bool via_sends_only)
{
	insert (_from_to, from, to);
	insert (_to_from, to, from);

	EdgeMapWithSends::iterator i = find_in_from_to_with_sends (from, to);
	if (i != _from_to_with_sends.end ()) {
		i->second.second = via_sends_only;
	} else {
		_from_to_with_sends.insert (
			make_pair (from, make_pair (to, via_sends_only))
			);
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

/** @return the vertices that are fed from `r' */
set<GraphVertex>
GraphEdges::from (GraphVertex r) const
{
	EdgeMap::const_iterator i = _from_to.find (r);
	if (i == _from_to.end ()) {
		return set<GraphVertex> ();
	}
	
	return i->second;
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
	for (EdgeMap::const_iterator i = _from_to.begin(); i != _from_to.end(); ++i) {
		cout << "FROM: " << i->first->name() << " ";
		for (set<GraphVertex>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
			cout << (*j)->name() << " ";
		}
		cout << "\n";
	}
	
	for (EdgeMap::const_iterator i = _to_from.begin(); i != _to_from.end(); ++i) {
		cout << "TO: " << i->first->name() << " ";
		for (set<GraphVertex>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
			cout << (*j)->name() << " ";
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
	bool operator () (GraphVertex r1, GraphVertex r2) const
	{
		if (r1->record_enabled()) {
			if (r2->record_enabled()) {
				/* both rec-enabled, just use signal order */
				return r1->order_key (MixerSort) < r2->order_key (MixerSort);
			} else {
				/* r1 rec-enabled, r2 not rec-enabled, run r2 early */
				return false;
			}
		} else {
			if (r2->record_enabled()) {
				/* r2 rec-enabled, r1 not rec-enabled, run r1 early */
				return true;
			} else {
				/* neither rec-enabled, use signal order */
				return r1->order_key (MixerSort) < r2->order_key (MixerSort);
			}
		}
	}
};

/** Perform a topological sort of a list of routes using a directed graph representing connections.
 *  @return Sorted list of routes, or 0 if the graph contains cycles (feedback loops).
 */
boost::shared_ptr<RouteList>
ARDOUR::topological_sort (
	boost::shared_ptr<RouteList> routes,
	GraphEdges edges
	)
{
	boost::shared_ptr<RouteList> sorted_routes (new RouteList);
	
	/* queue of routes to process */
	RouteList queue;

	/* initial queue has routes that are not fed by anything */
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		if (edges.has_none_to (*i)) {
			queue.push_back (*i);
		}
	}

	/* Sort the initial queue so that non-rec-enabled routes are run first.
	   This is so that routes can record things coming from other routes
	   via external connections.
	*/
	queue.sort (RouteRecEnabledComparator ());

	/* Do the sort: algorithm is Kahn's from Wikipedia.
	   `Topological sorting of large networks', Communications of the ACM 5(11):558-562.
	*/
	
	while (!queue.empty ()) {
		GraphVertex r = queue.front ();
		queue.pop_front ();
		sorted_routes->push_back (r);
		set<GraphVertex> e = edges.from (r);
		for (set<GraphVertex>::iterator i = e.begin(); i != e.end(); ++i) {
			edges.remove (r, *i);
			if (edges.has_none_to (*i)) {
				queue.push_back (*i);
			}
		}
	}

	if (!edges.empty ()) {
		edges.dump ();
		/* There are cycles in the graph, so we can't do a topological sort */
		return boost::shared_ptr<RouteList> ();
	}

	return sorted_routes;
}
