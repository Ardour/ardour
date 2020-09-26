/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk_ardour_automation_region_view_h__
#define __gtk_ardour_automation_region_view_h__

#include "ardour/types.h"

#include "region_view.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "enums.h"

namespace ARDOUR {
	class AutomationList;
	class Parameter;
};

class TimeAxisView;

class AutomationRegionView : public RegionView
{
public:
	AutomationRegionView(ArdourCanvas::Container*,
	                     AutomationTimeAxisView&,
	                     boost::shared_ptr<ARDOUR::Region>,
	                     const Evoral::Parameter& parameter,
	                     boost::shared_ptr<ARDOUR::AutomationList>,
	                     double initial_samples_per_pixel,
	                     uint32_t basic_color);

	~AutomationRegionView();

	void init (bool wfd);

	bool paste (Temporal::timepos_t const &                     pos,
	            unsigned                                        paste_count,
	            float                                           times,
	            boost::shared_ptr<const ARDOUR::AutomationList> slist);

	inline AutomationTimeAxisView* automation_view() const
		{ return dynamic_cast<AutomationTimeAxisView*>(&trackview); }

	boost::shared_ptr<AutomationLine> line() { return _line; }

	// We are a ghost.  Meta ghosts?  Crazy talk.
	virtual GhostRegion* add_ghost(TimeAxisView&) { return 0; }

	uint32_t get_fill_color() const;

	void set_height (double);
	void reset_width_dependent_items(double pixel_width);

protected:
	void create_line(boost::shared_ptr<ARDOUR::AutomationList> list);
	bool set_position(Temporal::timepos_t const & pos, void* src, double* ignored);
	void region_resized (const PBD::PropertyChange&);
	bool canvas_group_event(GdkEvent* ev);
	void add_automation_event (GdkEvent* event, Temporal::timepos_t const & when, double y, bool with_guard_points);
	void mouse_mode_changed ();
	void entered();
	void exited();

private:
	Evoral::Parameter                   _parameter;
	boost::shared_ptr<AutomationLine>   _line;
	PBD::ScopedConnection               _mouse_mode_connection;
};

#endif /* __gtk_ardour_automation_region_view_h__ */
