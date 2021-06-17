/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017-2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2017 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Tim Mayberry <mojofunk@gmail.com>
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

#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"

#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/tempo.h"
#include "ardour/transport_master_manager.h"
#include "ardour/utils.h"

#include "control_protocol/basic_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PBD::Signal2<void,std::string,std::string> BasicUI::AccessAction;

BasicUI::BasicUI (Session& s)
	: _session (&s)
	, _controller (&s)
{
}

BasicUI::~BasicUI ()
{

}

void
BasicUI::register_thread (std::string name)
{
	std::string pool_name = name;
	pool_name += " events";

	SessionEvent::create_per_thread_pool (pool_name, 64);
}

void
BasicUI::access_action ( std::string action_path )
{
	int split_at = action_path.find( "/" );
	std::string group = action_path.substr( 0, split_at );
	std::string item = action_path.substr( split_at + 1 );

	AccessAction( group, item );
}

bool
BasicUI::stop_button_onoff () const
{
	return _session->transport_stopped_or_stopping ();
}

bool
BasicUI::play_button_onoff () const
{
	return _controller.get_transport_speed() == 1.0;
}

bool
BasicUI::ffwd_button_onoff () const
{
	return _controller.get_transport_speed() > 1.0;
}

bool
BasicUI::rewind_button_onoff () const
{
	return _controller.get_transport_speed() < 0.0;
}

bool
BasicUI::loop_button_onoff () const
{
	return _session->get_play_loop();
}

void
BasicUI::undo ()
{
	access_action ("Editor/undo");
}

void
BasicUI::redo ()
{
	access_action ("Editor/redo");
}

void BasicUI::mark_in () { access_action("Common/start-range-from-playhead"); }
void BasicUI::mark_out () { access_action("Common/finish-range-from-playhead"); }

void BasicUI::set_punch_range () { access_action("Editor/set-punch-from-edit-range"); }
void BasicUI::set_loop_range () { access_action("Editor/set-loop-from-edit-range"); }
void BasicUI::set_session_range () { access_action("Editor/set-session-from-edit-range"); }

void BasicUI::quick_snapshot_stay () { access_action("Main/QuickSnapshotStay"); }
void BasicUI::quick_snapshot_switch () { access_action("Main/QuickSnapshotSwitch"); }

void BasicUI::fit_1_track() { access_action("Editor/fit_1_track"); }
void BasicUI::fit_2_tracks() { access_action("Editor/fit_2_tracks"); }
void BasicUI::fit_4_tracks() { access_action("Editor/fit_4_tracks"); }
void BasicUI::fit_8_tracks() { access_action("Editor/fit_8_tracks"); }
void BasicUI::fit_16_tracks() { access_action("Editor/fit_16_tracks"); }
void BasicUI::fit_32_tracks() { access_action("Editor/fit_32_tracks"); }
void BasicUI::fit_all_tracks() { access_action("Editor/fit_all_tracks"); }

void BasicUI::zoom_10_ms() { access_action("Editor/zoom_10_ms"); }
void BasicUI::zoom_100_ms() { access_action("Editor/zoom_100_ms"); }
void BasicUI::zoom_1_sec() { access_action("Editor/zoom_1_sec"); }
void BasicUI::zoom_10_sec() { access_action("Editor/zoom_10_sec"); }
void BasicUI::zoom_1_min() { access_action("Editor/zoom_1_min"); }
void BasicUI::zoom_5_min() { access_action("Editor/zoom_5_min"); }
void BasicUI::zoom_10_min() { access_action("Editor/zoom_10_min"); }
void BasicUI::zoom_to_session() { access_action("Editor/zoom-to-session"); }
void BasicUI::temporal_zoom_in() { access_action("Editor/temporal-zoom-in"); }
void BasicUI::temporal_zoom_out() { access_action("Editor/temporal-zoom-out"); }

void BasicUI::scroll_up_1_track() { access_action("Editor/step-tracks-up"); }
void BasicUI::scroll_dn_1_track() { access_action("Editor/step-tracks-down"); }
void BasicUI::scroll_up_1_page() { access_action("Editor/scroll-tracks-up"); }
void BasicUI::scroll_dn_1_page() { access_action("Editor/scroll-tracks-down"); }
