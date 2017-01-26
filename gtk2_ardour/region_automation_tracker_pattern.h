/*
    Copyright (C) 2016 Nil Geisweiller

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

#ifndef __ardour_gtk2_region_automation_tracker_pattern_h_
#define __ardour_gtk2_region_automation_tracker_pattern_h_

#include "automation_tracker_pattern.h"

/**
 * Data structure holding the automation list pattern held by a region.
 */
class RegionAutomationTrackerPattern : public AutomationTrackerPattern {
public:
	RegionAutomationTrackerPattern(ARDOUR::Session* session,
	                               boost::shared_ptr<ARDOUR::Region> region,
	                               const AutomationControlSet& automation_controls);

	// Assign a control event to a row
	virtual uint32_t control_event2row(const Evoral::Parameter& param, const Evoral::ControlEvent* event);
};

#endif /* __ardour_gtk2_region_automation_tracker_pattern_h_ */
