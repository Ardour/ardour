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

#ifndef __ardour_gtk_automation_selectable_h__
#define __ardour_gtk_automation_selectable_h__

#include "ardour/types.h"
#include "selectable.h"

class TimeAxisView;

/** A selected automation point, expressed as a rectangle on a track (so that x coordinates
 *  are frames and y coordinates are a fraction of track height).  This representation falls
 *  between the visible GUI control points and the back-end "actual" automation points,
 *  some of which may not be visible; it is not trivial to convert from one of these to the other,
 *  so the AutomationSelectable is a kind of "best and worst of both worlds".
 */
struct AutomationSelectable : public Selectable
{
    nframes_t start;
    nframes_t end;
    double low_fract;
    double high_fract;
    TimeAxisView* track; // ref would be better, but ARDOUR::SessionHandlePtr is non-assignable

    AutomationSelectable (nframes_t s, nframes_t e, double l, double h, TimeAxisView* atv)
	    : start (s), end (e), low_fract (l), high_fract (h), track (atv) {}

    bool operator== (const AutomationSelectable& other) {
	    return start == other.start &&
	    end == other.end &&
	    low_fract == other.low_fract &&
	    high_fract == other.high_fract &&
	    track == other.track;
    }
};

#endif /* __ardour_gtk_automation_selectable_h__ */
