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

#include "ardour/rc_configuration.h"

#include "canvas/canvas.h"

#include "ardour_ui.h"
#include "editor.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "route_time_axis.h"
#include "audio_region_view.h"
#include "selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::start_updating_meters ()
{
	RouteTimeAxisView* rtv;

	if (is_mapped() && _session) {
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
				rtv->reset_meter ();
			}
		}
	}

	meters_running = true;
}

void
Editor::stop_updating_meters ()
{
	RouteTimeAxisView* rtv;

	meters_running = false;

	if (is_mapped() && _session) {
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
				rtv->hide_meter ();
			}
		}
	}
}

void
Editor::toggle_meter_updating()
{
	if (Config->get_show_track_meters()) {
		start_updating_meters ();
	} else {
		stop_updating_meters ();
	}

	track_canvas_viewport_allocate (track_canvas->get_allocation());
}

