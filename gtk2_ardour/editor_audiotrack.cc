/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#include "ardour/rc_configuration.h"

#include "canvas/canvas.h"

#include "editor.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "route_time_axis.h"
#include "audio_region_view.h"
#include "selection.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::start_updating_meters ()
{
	RouteTimeAxisView* rtv;

	if (contents().is_mapped() && _session) {
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

	if (contents().is_mapped() && _session) {
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
	DisplaySuspender ds;
	if (UIConfiguration::instance().get_show_track_meters()) {
		start_updating_meters ();
	} else {
		stop_updating_meters ();
	}

	track_canvas_viewport_allocate (_track_canvas->get_allocation());
}

