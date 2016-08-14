/*
    Copyleft (C) 2015 Nil Geisweiller

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

#ifndef __ardour_gtk2_automation_tracker_pattern_h_
#define __ardour_gtk2_automation_tracker_pattern_h_

#include <set>

#include "ardour/automation_control.h"

#include "tracker_pattern.h"

namespace ARDOUR {
	class Session;
	class AutomationControl;
};

typedef std::set<boost::shared_ptr<ARDOUR::AutomationControl> > AutomationControlSet;

/**
 * Data structure holding the automation list pattern.
 */
class AutomationTrackerPattern : public TrackerPattern {
public:
	typedef ARDOUR::AutomationList::const_iterator AutomationListConstIt;
	typedef std::multimap<uint32_t, AutomationListConstIt> RowToAutomationIt;

	AutomationTrackerPattern(ARDOUR::Session* session,
	                         boost::shared_ptr<ARDOUR::Region> region,
	                         const AutomationControlSet& automation_controls);

	// Build or rebuild the pattern (implement TrackerPattern::update_pattern)
	void update_pattern();

	// Map parameters to maps of row to automation range
	std::map<Evoral::Parameter, RowToAutomationIt> automations;

private:
	AutomationControlSet _automation_controls;
};

#endif /* __ardour_gtk2_automation_tracker_pattern_h_ */
