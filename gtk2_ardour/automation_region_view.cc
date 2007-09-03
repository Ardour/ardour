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

#include "automation_region_view.h"

AutomationRegionView::AutomationRegionView(ArdourCanvas::Group*                      parent,
                                           AutomationTimeAxisView&                   time_axis,
                                           boost::shared_ptr<ARDOUR::Region>         region,
                                           boost::shared_ptr<ARDOUR::AutomationList> list,
                                           double                                    spu,
                                           Gdk::Color&                               basic_color)
	: RegionView(parent, time_axis, region, spu, basic_color)
	, _line(list->parameter().to_string(), time_axis, *group, list)
{ 
	_line.set_colors();
	_line.show();
	_line.show_all_control_points();
	
	group->raise_to_top ();
	
	group->signal_event().connect (mem_fun (this, &AutomationRegionView::canvas_event), false);
}


bool
AutomationRegionView::canvas_event(GdkEvent* ev)
{
	cerr << "AUTOMATION EVENT" << endl;

	return false;
}


void
AutomationRegionView::set_y_position_and_height (double y, double h)
{
	RegionView::set_y_position_and_height(y, h - 1);

	_line.set_y_position_and_height ((uint32_t)y, (uint32_t) rint (h - NAME_HIGHLIGHT_SIZE));
}

void
AutomationRegionView::region_resized (ARDOUR::Change what_changed)
{
	// Do nothing, parent will move us
}


void
AutomationRegionView::entered()
{
	_line.track_entered();
}


void
AutomationRegionView::exited()
{
	_line.track_exited();
}

