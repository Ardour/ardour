#ifndef __ardour_gtk_track_selection_h__
#define __ardour_gtk_track_selection_h__

#include <list>

class TimeAxisView;

struct TrackSelection : public list<TimeAxisView*> {};

#endif /* __ardour_gtk_track_selection_h__ */
