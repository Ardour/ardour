#ifndef __ardour_gtk_automation_selectable_h__
#define __ardour_gtk_automation_selectable_h__

#include <ardour/types.h>
#include "selectable.h"

class TimeAxisView;

struct AutomationSelectable : public Selectable
{
    nframes_t start;
    nframes_t end;
    double low_fract;
    double high_fract;
    TimeAxisView& track;

    AutomationSelectable (nframes_t s, nframes_t e, double l, double h, TimeAxisView& atv)
	    : start (s), end (e), low_fract (l), high_fract (h), track (atv) {}

    bool operator== (const AutomationSelectable& other) {
	    return start == other.start &&
		    end == other.end &&
		    low_fract == other.low_fract &&
		    high_fract == other.high_fract &&
		    &track == &other.track;
    }
};

#endif /* __ardour_gtk_automation_selectable_h__ */
