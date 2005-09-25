#ifndef __ardour_gtk_time_selection_h__
#define __ardour_gtk_time_selection_h__

#include <list>
#include <ardour/types.h>

namespace ARDOUR {
	class RouteGroup;
}

class TimeAxisView;

struct TimeSelection : public std::list<ARDOUR::AudioRange> {
    
    /* if (track == 0 && group == 0) then it applies to all
       tracks.

       if (track != 0 && group == 0) then it applies just to 
       that track.

       if (group != 0) then it applies to all tracks in 
       the group.
    */
    
    TimeAxisView*               track;
    ARDOUR::RouteGroup*         group;

    ARDOUR::AudioRange& operator[](uint32_t);

    jack_nframes_t start();
    jack_nframes_t end_frame();
    jack_nframes_t length();

    bool consolidate ();
};


#endif /* __ardour_gtk_time_selection_h__ */
