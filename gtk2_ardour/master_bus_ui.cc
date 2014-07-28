/*
	Copyright (C) 2014 Waves Audio Ltd.

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

#include "meter_patterns.h"
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
using namespace ArdourMeter;

int MasterBusUI::__meter_width = 6;
PBD::Signal1<void,MasterBusUI*> MasterBusUI::CatchDeletion;

MasterBusUI::MasterBusUI (Session* sess)
	: WavesUI ("master_ui.xml", *this)
	, _max_peak (minus_infinity())
	, _peak_treshold (xml_property(*xml_tree()->root(), "peaktreshold", -144.4)) // Think about having it in config
	, _level_meter_home (get_box ("level_meter_home"))
	, _level_meter (sess)
	, _peak_display_button (get_waves_button ("peak_display_button"))
	, _master_mute_button (get_waves_button ("master_mute_button"))
	, _clear_solo_button (get_waves_button ("clear_solo_button"))
	, _global_rec_button (get_waves_button ("global_rec_button"))
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	_level_meter_home.pack_start (_level_meter, true, true);
	_peak_display_button.unset_flags (Gtk::CAN_FOCUS);
	_master_mute_button.unset_flags (Gtk::CAN_FOCUS);
	_clear_solo_button.unset_flags (Gtk::CAN_FOCUS);
	_global_rec_button.unset_flags (Gtk::CAN_FOCUS);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_group_peak_display));

	_peak_display_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_peak_display_button));
	_master_mute_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_master_mute_button));
	_clear_solo_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_clear_solo_button));
	_global_rec_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_global_rec_button));
}

MasterBusUI::~MasterBusUI ()
{
	CatchDeletion (this);
}

void
MasterBusUI::set_route (boost::shared_ptr<Route> rt)
{
	reset ();
	_route = rt;
	_level_meter.set_meter (_route->shared_peak_meter().get());
	_level_meter.clear_meters();
	_level_meter.set_type (_route->meter_type());
	_level_meter.setup_meters (__meter_width, __meter_width);
	_route->shared_peak_meter()->ConfigurationChanged.connect (_route_connections,
		                                                       invalidator (*this),
															   boost::bind (&MasterBusUI::meter_configuration_changed, 
															                this,
																			_1), 
															   gui_context());
	_route->DropReferences.connect (_route_connections,
									invalidator (*this),
									boost::bind (&MasterBusUI::reset,
												 this),
									gui_context());
}

void
MasterBusUI::reset ()
{
	_route_connections.drop_connections ();
	_route = boost::shared_ptr<ARDOUR::Route>(); // It's to have it "false"
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
		float mpeak = _level_meter.update_meters();
		if (mpeak > _max_peak) {
			_max_peak = mpeak;
			if (mpeak >= Config->get_meter_peak()) {
				_peak_display_button.set_active_state (Gtkmm2ext::ExplicitActive);
			}
			char buf[32];
			if (mpeak <= _peak_treshold) {
				_peak_display_button.set_text ("- inf");
			} else {
				snprintf (buf, sizeof(buf), "%.1f", mpeak);
				_peak_display_button.set_text (buf);
			}
		}
 	}
}

void
MasterBusUI::meter_configuration_changed (ChanCount c)
{
	_level_meter.setup_meters (__meter_width, __meter_width);
}

void
MasterBusUI::reset_peak_display ()
{
	_level_meter.clear_meters();
	_max_peak = -INFINITY;
	_peak_display_button.set_text (_("- inf"));
	_peak_display_button.set_active_state(Gtkmm2ext::Off);
}

void
MasterBusUI::reset_route_peak_display (Route* route)
{
	if (_route && _route.get() == route) {
		reset_peak_display ();
	}
}

void
MasterBusUI::reset_group_peak_display (RouteGroup* group)
{
	if (_route && group == _route->route_group()) {
		reset_peak_display ();
	}
}

void 
MasterBusUI::on_peak_display_button (WavesButton*)
{
	if (_route) {
		ResetRoutePeakDisplays (_route.get());
	}
}

void MasterBusUI::on_master_mute_button (WavesButton*)
{
}

void MasterBusUI::on_clear_solo_button (WavesButton*)
{
}

void MasterBusUI::on_global_rec_button (WavesButton*)
{
}
