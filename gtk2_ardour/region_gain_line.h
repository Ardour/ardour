/*
 * Copyright (C) 2005-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_region_gain_line_h__
#define __ardour_gtk_region_gain_line_h__

#include "ardour/ardour.h"

#include "region_fx_line.h"

namespace ARDOUR {
	class Session;
}

class TimeAxisView;
class AudioRegionView;

class AudioRegionGainLine : public RegionFxLine
{
public:
	AudioRegionGainLine (const std::string & name, AudioRegionView&, ArdourCanvas::Container& parent, std::shared_ptr<ARDOUR::AutomationList>);

	void start_drag_single (ControlPoint*, double, float);
	void start_drag_line (uint32_t, uint32_t, float);
	void start_drag_multiple (std::list<ControlPoint*>, float, XMLNode*);
	void end_drag (bool with_push, uint32_t final_index);
	void end_draw_merge ();
	void enable_autoation ();
	void remove_point (ControlPoint&);

private:
	AudioRegionView& arv;
};

#endif /* __ardour_gtk_region_gain_line_h__ */
