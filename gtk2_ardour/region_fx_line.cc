/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include "ardour/automation_control.h"

#include "audio_region_view.h"
#include "gui_thread.h"
#include "region_fx_line.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RegionFxLine::RegionFxLine (std::string const& name, RegionView& r, ArdourCanvas::Container& parent, std::shared_ptr<AutomationList> l, ParameterDescriptor const& d)
	: AutomationLine (name, r.get_time_axis_view(), parent, l, d)
	, _rv (r)
{
	terminal_points_can_slide = false;
	init ();
}

RegionFxLine::RegionFxLine (std::string const& name, RegionView& r, ArdourCanvas::Container& parent, std::shared_ptr<ARDOUR::AutomationControl> ac)
	: AutomationLine (name, r.get_time_axis_view(), parent, ac->alist (), ac->desc ())
	, _rv (r)
	, _ac (ac)
{
	terminal_points_can_slide = false;
	init ();
}

void
RegionFxLine::init ()
{
	_rv.region()->PropertyChanged.connect (_region_changed_connection, invalidator (*this), boost::bind (&RegionFxLine::region_changed, this, _1), gui_context());
	group->raise_to_top ();
	group->set_y_position (2);
}

Temporal::timepos_t
RegionFxLine::get_origin() const
{
	return _rv.region()->position();
}

void
RegionFxLine::enable_autoation ()
{
	std::shared_ptr<AutomationControl> ac = _ac.lock ();
	if (ac) {
		ac->set_automation_state (Play);
	}
}

void
RegionFxLine::end_drag (bool with_push, uint32_t final_index)
{
	enable_autoation ();
	AutomationLine::end_drag (with_push, final_index);
}

void
RegionFxLine::end_draw_merge ()
{
	enable_autoation ();
	AutomationLine::end_draw_merge ();
}

void
RegionFxLine::region_changed (PBD::PropertyChange const& what_changed)
{
	PBD::PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::length);

	if (what_changed.contains (interesting_stuff)) {
		reset ();
	}
}
