/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_automation_streamview_h__
#define __ardour_automation_streamview_h__

#include <list>
#include <cmath>

#include "ardour/location.h"
#include "enums.h"
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
	ARDOUR::AutoState automation_state () const;

	void redisplay_track ();

	inline double contents_height() const {
		return (_trackview.current_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2);
	}

	bool has_automation () const;

	void set_interpolation (ARDOUR::AutomationList::InterpolationStyle);
	ARDOUR::AutomationList::InterpolationStyle interpolation () const;

	void clear ();

	void get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<Selectable*> &, bool within = false);
	void set_selected_points (PointSelection &);

	std::list<boost::shared_ptr<AutomationLine> > get_lines () const;

	bool paste (Temporal::timepos_t const &               pos,
	            unsigned                                  paste_count,
	            float                                     times,
	            boost::shared_ptr<ARDOUR::AutomationList> list);

private:
	void setup_rec_box ();

	RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_data, bool recording = false);
	void        display_region(AutomationRegionView* region_view);

	void color_handler ();

	AutomationTimeAxisView& _automation_view;
	/** automation state that should be applied when this view gets its first RegionView */
	ARDOUR::AutoState _pending_automation_state;
};

#endif /* __ardour_automation_streamview_h__ */
