/*
    Copyright (C) 2006 Paul Davis

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

#include <cstdlib>
#include <cmath>
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <utility>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "ardour/amp.h"
#include "ardour/meter.h"
#include "ardour/event_type_map.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/audio_track.h"

#include "evoral/Parameter.hpp"

#include "canvas/debug.h"

#include "ardour_ui.h"
#include "ardour_button.h"
#include "debug.h"
#include "global_signals.h"
#include "master_bus_ui.h"
#include "enums.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "utils.h"
#include "route_group_menu.h"

#include "ardour/track.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Editing;
using namespace std;
using std::list;

PBD::Signal1<void,MasterBusUI*> MasterBusUI::CatchDeletion;

MasterBusUI::MasterBusUI (PublicEditor& ed,
						  Session* sess)
	: AxisView (sess)
	, RouteUI (sess, "master_ui.xml")
	, peak_display_button (get_waves_button ("peak_display_button"))
	, level_meter_home (get_box ("level_meter_home"))
{
	level_meter = new LevelMeterHBox(sess);
	//level_meter->ButtonRelease.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::level_meter_button_release, this, _1));
	//level_meter->MeterTypeChanged.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::meter_type_changed, this, _1));
	level_meter_home.pack_start (*level_meter, true, true);
	peak_display_button.unset_flags (Gtk::CAN_FOCUS);
}

MasterBusUI::~MasterBusUI ()
{
	if (level_meter) {
		delete level_meter;
		CatchDeletion (this);
	}
}

void
MasterBusUI::set_route (boost::shared_ptr<Route> rt)
{
	level_meter->set_meter (rt->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->set_type (rt->meter_type());
	level_meter->setup_meters (6, 6);
    RouteUI::set_route(rt);
}

void MasterBusUI::set_button_names ()
{
}

void
MasterBusUI::fast_update ()
{
	if (_route) {
		Gtk::Requisition sz;
		size_request (sz);
		if (sz.height == 0) {
			return;
		}
		float mpeak = level_meter->update_meters();
/*
		if (mpeak > max_peak) {
			max_peak = mpeak;
			if (mpeak >= Config->get_meter_peak()) {
				peak_display.set_name ("meterbridge peakindicator on");
			}
		}
 */
	}
}

std::string
MasterBusUI::state_id () const
{
	return string_compose ("master %1", _route->id().to_s());
}
