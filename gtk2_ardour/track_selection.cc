#include <algorithm>
#include "track_selection.h"

using namespace std;

list<TimeAxisView*>
TrackSelection::add (list<TimeAxisView*> const & t)
{
	list<TimeAxisView*> added;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (find (begin(), end(), *i) == end()) {
			added.push_back (*i);
			push_back (*i);
		}
	}

	return added;
}
