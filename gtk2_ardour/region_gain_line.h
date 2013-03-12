/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_region_gain_line_h__
#define __ardour_gtk_region_gain_line_h__

#include "ardour/ardour.h"

#include <libgnomecanvasmm.h>

#include "automation_line.h"

namespace ARDOUR {
	class Session;
}

class TimeAxisView;
class AudioRegionView;

class AudioRegionGainLine : public AutomationLine
{
  public:
	AudioRegionGainLine (const std::string & name, AudioRegionView&, ArdourCanvas::Group& parent, boost::shared_ptr<ARDOUR::AutomationList>);

        void start_drag_single (ControlPoint*, double, float);
        void end_drag (bool with_push, uint32_t final_index);

	void remove_point (ControlPoint&);

private:
	AudioRegionView& rv;
};


#endif /* __ardour_gtk_region_gain_line_h__ */
