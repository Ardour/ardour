/*
    Copyright (C) 2001, 2007 Paul Davis

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

#ifndef __ardour_automation_streamview_h__
#define __ardour_automation_streamview_h__

#include <list>
#include <cmath>

#include "ardour/location.h"
#include "enums.h"
#include "simplerect.h"
#include "streamview.h"
#include "time_axis_view_item.h"
#include "route_time_axis.h"
#include "automation_controller.h"

namespace Gdk {
	class Color;
}

class PublicEditor;
class Selectable;
class Selection;
class AutomationRegionView;

class AutomationStreamView : public StreamView
{
  public:
	AutomationStreamView (AutomationTimeAxisView& tv);
	~AutomationStreamView ();

	void set_automation_state (ARDOUR::AutoState state);

	void redisplay_diskstream ();

	inline double contents_height() const {
		return (_trackview.current_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2);
	}

  private:
	void setup_rec_box ();
	void rec_data_range_ready (jack_nframes_t start, jack_nframes_t dur);
	void update_rec_regions (jack_nframes_t start, jack_nframes_t dur);

	RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_data, bool recording = false);
	void        display_region(AutomationRegionView* region_view);

	void color_handler ();

	boost::shared_ptr<AutomationController> _controller;

	AutomationTimeAxisView& _automation_view;
};

#endif /* __ardour_automation_streamview_h__ */
