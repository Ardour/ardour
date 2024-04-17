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

#ifndef __ardour_gtk_region_fx_line_h__
#define __ardour_gtk_region_fx_line_h__

#include "automation_line.h"

class RegionView;

class RegionFxLine : public AutomationLine
{
public:
	RegionFxLine (std::string const&, RegionView&, ArdourCanvas::Container&, std::shared_ptr<ARDOUR::AutomationList>, ARDOUR::ParameterDescriptor const&);
	RegionFxLine (std::string const&, RegionView&, ArdourCanvas::Container&, std::shared_ptr<ARDOUR::AutomationControl>);

	Temporal::timepos_t get_origin() const;

	RegionView& region_view () { return _rv; }

	void end_drag (bool with_push, uint32_t final_index);
	void end_draw_merge ();

	virtual void enable_autoation ();

private:
	void init ();
	void region_changed (PBD::PropertyChange const&);

	RegionView&                              _rv;
	std::weak_ptr<ARDOUR::AutomationControl> _ac;
	PBD::ScopedConnection                    _region_changed_connection;
};

#endif
