/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#ifndef __gtk_ardour_automation_region_view_h__
#define __gtk_ardour_automation_region_view_h__

#include <ardour/diskstream.h>
#include <ardour/types.h>

#include "region_view.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "enums.h"
#include "canvas.h"

namespace ARDOUR {
	class AutomationList;
	class Parameter;
};

class TimeAxisView;

class AutomationRegionView : public RegionView
{
public:
	AutomationRegionView(ArdourCanvas::Group*,
	                     AutomationTimeAxisView&,
	                     boost::shared_ptr<ARDOUR::Region>,
	                     const ARDOUR::Parameter& parameter,
	                     boost::shared_ptr<ARDOUR::AutomationList>,
	                     double initial_samples_per_unit,
	                     Gdk::Color& basic_color);

	~AutomationRegionView() {}
	
	void init (Gdk::Color& basic_color, bool wfd);
	
	inline AutomationTimeAxisView* automation_view() const
		{ return dynamic_cast<AutomationTimeAxisView*>(&trackview); }
	
	void set_line(boost::shared_ptr<AutomationLine> line) { _line = line; }
	boost::shared_ptr<AutomationLine> line() { return _line; }
	
	// We are a ghost.  Meta ghosts?  Crazy talk.
	virtual GhostRegion* add_ghost(TimeAxisView&) { return NULL; }
	
	void set_height (double);
	void reset_width_dependent_items(double pixel_width);

protected:
	void create_line(boost::shared_ptr<ARDOUR::AutomationList> list);
	bool set_position(nframes_t pos, void* src, double* ignored);
	void region_resized(ARDOUR::Change what_changed);
	bool canvas_event(GdkEvent* ev);
	void add_automation_event (GdkEvent* event, nframes_t when, double y);
	void entered();
	void exited();

private:
	ARDOUR::Parameter                 _parameter;
	boost::shared_ptr<AutomationLine> _line;
};

#endif /* __gtk_ardour_automation_region_view_h__ */
