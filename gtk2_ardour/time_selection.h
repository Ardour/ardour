/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __ardour_gtk_time_selection_h__
#define __ardour_gtk_time_selection_h__

#include <list>
#include "ardour/types.h"
#include "shared_ptrs.h"

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
    
    TimeAxisViewPtr             track;
    ARDOUR::RouteGroup*         group;

    ARDOUR::AudioRange& operator[](uint32_t);

    nframes_t start();
    nframes_t end_frame();
    nframes_t length();

    bool consolidate ();
};


#endif /* __ardour_gtk_time_selection_h__ */
