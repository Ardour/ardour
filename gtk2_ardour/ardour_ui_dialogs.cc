/*
    Copyright (C) 2000 Paul Davis 

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

/* This file contains any ARDOUR_UI methods that require knowledge of
   the various dialog boxes, and exists so that no compilation dependency 
   exists between the main ARDOUR_UI modules and their respective classes.
   This is to cut down on the compile times.  It also helps with my sanity.
*/

#include <ardour/session.h>

#include "actions.h"
#include "ardour_ui.h"
#include "connection_editor.h"
#include "location_ui.h"
#include "mixer_ui.h"
#include "option_editor.h"
#include "public_editor.h"
#include "route_params_ui.h"
#include "sfdb_ui.h"
#include "color_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;

void
ARDOUR_UI::connect_to_session (Session *s)
{
	session = s;

	session->HaltOnXrun.connect (mem_fun(*this, &ARDOUR_UI::halt_on_xrun_message));

	/* sensitize menu bar options that are now valid */

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, true);
	
	if (session->locations()->num_range_markers()) {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
	} else {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
	}

	/* there are never any selections on startup */

	ActionManager::set_sensitive (ActionManager::region_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::line_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::point_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::playlist_selection_sensitive_actions, false);

	session->locations()->added.connect (mem_fun (*this, &ARDOUR_UI::handle_locations_change));
	session->locations()->removed.connect (mem_fun (*this, &ARDOUR_UI::handle_locations_change));

	rec_button.set_sensitive (true);
	shuttle_box.set_sensitive (true);
	
	if (session->n_diskstreams() == 0) {
		session->DiskStreamAdded.connect (mem_fun(*this, &ARDOUR_UI::diskstream_added));
	}

	if (connection_editor) {
		connection_editor->set_session (s);
	}

	if (location_ui) {
		location_ui->set_session(s);
	}

	if (route_params) {
		route_params->set_session (s);
	}

	if (option_editor) {
		option_editor->set_session (s);
	}

	if (sfdb) {
		sfdb->set_session (s);
	}

	setup_session_options ();

	Blink.connect (mem_fun(*this, &ARDOUR_UI::transport_rec_enable_blink));
	Blink.connect (mem_fun(*this, &ARDOUR_UI::solo_blink));
	Blink.connect (mem_fun(*this, &ARDOUR_UI::audition_blink));

	/* these are all need to be handled in an RT-safe and MT way, so don't
	   do any GUI work, just queue it for handling by the GUI thread.
	*/

	session->TransportStateChange.connect (mem_fun(*this, &ARDOUR_UI::queue_transport_change));

	/* alert the user to these things happening */

	session->AuditionActive.connect (mem_fun(*this, &ARDOUR_UI::auditioning_changed));
	session->SoloActive.connect (mem_fun(*this, &ARDOUR_UI::soloing_changed));

	solo_alert_button.set_active (session->soloing());

	/* can't be auditioning here */

	primary_clock.set_session (s);
	secondary_clock.set_session (s);
	big_clock.set_session (s);
	preroll_clock.set_session (s);
	postroll_clock.set_session (s);

	/* Clocks are on by default after we are connected to a session, so show that here.
	*/
	
	connect_dependents_to_session (s);

	start_clocking ();
	start_blinking ();

	if (editor) {
		editor->present();
	}

	transport_stopped ();

	second_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::every_second), 1000);
	point_one_second_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::every_point_one_seconds), 100);
	point_zero_one_second_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::every_point_zero_one_seconds), 40);
}

int
ARDOUR_UI::unload_session ()
{
	if (session && session->dirty()) {
		switch (ask_about_saving_session (_("close session"))) {
		case -1:
			return 1;
			
		case 1:
			session->save_state ("");
			break;
		}
	}

	second_connection.disconnect ();
	point_one_second_connection.disconnect ();
	point_zero_one_second_connection.disconnect();

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);
	
	rec_button.set_sensitive (false);
	shuttle_box.set_sensitive (false);

	stop_blinking ();
	stop_clocking ();

	/* drop everything attached to the blink signal */

	Blink.clear ();

	primary_clock.set_session (0);
	secondary_clock.set_session (0);
	big_clock.set_session (0);
	preroll_clock.set_session (0);
	postroll_clock.set_session (0);

	if (option_editor) {
		option_editor->set_session (0);
	}

	if (mixer) {
		mixer->hide_all ();
	}

	delete session;
	session = 0;

	update_buffer_load ();
	// update_disk_rate ();

	return 0;
}

int
ARDOUR_UI::create_connection_editor ()
{
#if 0
	if (connection_editor == 0) {
		connection_editor = new ConnectionEditor ();
		connection_editor->signal_unmap().connect (sigc::bind (ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleConnections")));
	}

	if (session) {
		connection_editor->set_session (session);
	}
#endif

	return 0;
}

void
ARDOUR_UI::toggle_connection_editor ()
{
	if (create_connection_editor()) {
		return;
	}

#if 0
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleConnections"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			connection_editor->show_all ();
			connection_editor->present ();
		} else {
			connection_editor->hide ();
		} 
	}
#endif
}

void
ARDOUR_UI::toggle_big_clock_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleBigClock"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			big_clock_window->show_all ();
			big_clock_window->present ();
		} else {
			big_clock_window->hide ();
		} 
	}
}

void
ARDOUR_UI::toggle_options_window ()
{
	if (option_editor == 0) {
		option_editor = new OptionEditor (*this, *editor, *mixer);
		option_editor->signal_unmap().connect(sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleOptionsEditor")));
		option_editor->set_session (session);
	} 

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleOptionsEditor"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			option_editor->show_all ();
			option_editor->present ();
		} else {
			option_editor->hide ();
		} 
	}
}

int
ARDOUR_UI::create_location_ui ()
{
	if (location_ui == 0) {
		location_ui = new LocationUI ();
		location_ui->set_session (session);
		location_ui->signal_unmap().connect (sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleLocations")));
	}
	return 0;
}

void
ARDOUR_UI::toggle_location_window ()
{
	if (create_location_ui()) {
		return;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleLocations"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			location_ui->show_all ();
			location_ui->present ();
		} else {
			location_ui->hide ();
		} 
	}
}

void
ARDOUR_UI::toggle_color_manager ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleColorManager"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			color_manager->show_all ();
			color_manager->present ();
		} else {
			color_manager->hide ();
		} 
	}
}

int
ARDOUR_UI::create_route_params ()
{
	if (route_params == 0) {
		route_params = new RouteParams_UI (*engine);
		route_params->set_session (session);
		route_params->signal_unmap().connect (sigc::bind(sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleInspector")));
	}
	return 0;
}

void
ARDOUR_UI::toggle_route_params_window ()
{
	if (create_route_params ()) {
		return;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleInspector"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			route_params->show_all ();
			route_params->present ();
		} else {
			route_params->hide ();
		} 
	}
}

int
ARDOUR_UI::create_sound_file_browser ()
{
	if (sfdb == 0) {
		sfdb = new SoundFileBrowser (_("Sound File Browser"));
		sfdb->set_session (session);
		sfdb->signal_unmap().connect (sigc::bind(sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleSoundFileBrowser")));
	}
	return 0;
}
	
void
ARDOUR_UI::toggle_sound_file_browser ()
{
	if (create_sound_file_browser()) {
		return;
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleSoundFileBrowser"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
	
		if (tact->get_active()) {
			sfdb->show_all();
			sfdb->present();
		} else {
			sfdb->hide ();
		}
	}
}

void
ARDOUR_UI::handle_locations_change (Location* ignored)
{
	if (session) {
		if (session->locations()->num_range_markers()) {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
		} else {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
		}
	}
}
