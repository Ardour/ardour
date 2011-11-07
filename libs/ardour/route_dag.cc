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
#include "ardour/route_dag.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

void
DAGEdges::add (boost::shared_ptr<Route> from, boost::shared_ptr<Route> to)
{
	insert (_from_to, from, to);
	insert (_to_from, to, from);
	
	EdgeMap::iterator i = _from_to.find (from);
	if (i != _from_to.end ()) {
		i->second.insert (to);
	} else {
		set<boost::shared_ptr<Route> > v;
		v.insert (to);
		_from_to.insert (make_pair (from, v));
	}
	
}

set<boost::shared_ptr<Route> >
DAGEdges::from (boost::shared_ptr<Route> r) const
{
	EdgeMap::const_iterator i = _from_to.find (r);
	if (i == _from_to.end ()) {
		return set<boost::shared_ptr<Route> > ();
	}
	
	return i->second;
}

void
DAGEdges::remove (boost::shared_ptr<Route> from, boost::shared_ptr<Route> to)
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
}

/** @param to `To' route.
 *  @return true if there are no edges going to `to'.
 */

bool
DAGEdges::has_none_to (boost::shared_ptr<Route> to) const
{
	return _to_from.find (to) == _to_from.end ();
}

bool
DAGEdges::empty () const
{
	assert (_from_to.empty () == _to_from.empty ());
	return _from_to.empty ();
}

void
DAGEdges::dump () const
{
	for (EdgeMap::const_iterator i = _from_to.begin(); i != _from_to.end(); ++i) {
		cout << "FROM: " << i->first->name() << " ";
		for (set<boost::shared_ptr<Route> >::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
			cout << (*j)->name() << " ";
		}
		cout << "\n";
	}
	
	for (EdgeMap::const_iterator i = _to_from.begin(); i != _to_from.end(); ++i) {
		cout << "TO: " << i->first->name() << " ";
		for (set<boost::shared_ptr<Route> >::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
			cout << (*j)->name() << " ";
		}
		cout << "\n";
	}
}
	
void
DAGEdges::insert (EdgeMap& e, boost::shared_ptr<Route> a, boost::shared_ptr<Route> b)
{
	EdgeMap::iterator i = e.find (a);
	if (i != e.end ()) {
		i->second.insert (b);
	} else {
		set<boost::shared_ptr<Route> > v;
		v.insert (b);
		e.insert (make_pair (a, v));
	}
}

struct RouteRecEnabledComparator
{
	bool operator () (boost::shared_ptr<Route> r1, boost::shared_ptr<Route> r2) const
	{
		if (r1->record_enabled()) {
			if (r2->record_enabled()) {
				/* both rec-enabled, just use signal order */
				return r1->order_key(N_("signal")) < r2->order_key(N_("signal"));
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
				return r1->order_key(N_("signal")) < r2->order_key(N_("signal"));
			}
		}
	}
};

		
boost::shared_ptr<RouteList>
ARDOUR::topological_sort (
	boost::shared_ptr<RouteList> routes,
	DAGEdges edges
	)
{
	boost::shared_ptr<RouteList> sorted_routes (new RouteList);
	
	/* queue of routes to process */
	RouteList queue;

	/* initial queue has routes that are not fed by anything */
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		if ((*i)->not_fed ()) {
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
		boost::shared_ptr<Route> r = queue.front ();
		queue.pop_front ();
		sorted_routes->push_back (r);
		set<boost::shared_ptr<Route> > e = edges.from (r);
		for (set<boost::shared_ptr<Route> >::iterator i = e.begin(); i != e.end(); ++i) {
			edges.remove (r, *i);
			if (edges.has_none_to (*i)) {
				queue.push_back (*i);
			}
		}
	}

	if (!edges.empty ()) {
		cout << "Feedback detected.\n";
	}

	return sorted_routes;
}
