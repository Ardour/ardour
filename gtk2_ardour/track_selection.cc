#include <algorithm>
#include "track_selection.h"

using namespace std;

TrackSelection::TrackSelection (list<TimeAxisViewPtr> const &t)
	: list<TimeAxisViewPtr> (t)
{

}

list<TimeAxisViewPtr>
TrackSelection::add (list<TimeAxisViewPtr> const & t)
{
	list<TimeAxisViewPtr> added;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (!contains (*i)) {
			added.push_back (*i);
			push_back (*i);
		}
	}

	return added;
}

bool
TrackSelection::contains (TimeAxisViewConstPtr t) const
{
	return find (begin(), end(), t) != end();
}
