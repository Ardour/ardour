#include <algorithm>
#include "track_selection.h"

using namespace std;

list<TimeAxisView*>
TrackSelection::add (list<TimeAxisView*> const & t)
{
	list<TimeAxisView*> added;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (!contains (*i)) {
			added.push_back (*i);
			push_back (*i);
		}
	}

	return added;
}

bool
TrackSelection::contains (TimeAxisView const * t) const
{
	return find (begin(), end(), t) != end();
}
