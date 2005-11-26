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

#include "ardour_ui.h"
#include "connection_editor.h"
#include "location_ui.h"
#include "meter_bridge.h"
#include "mixer_ui.h"
#include "option_editor.h"
#include "public_editor.h"
#include "route_params_ui.h"
#include "sfdb_ui.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

void
ARDOUR_UI::connect_to_session (Session *s)
{
	session = s;

	session->HaltOnXrun.connect (mem_fun(*this, &ARDOUR_UI::halt_on_xrun_message));

	/* sensitize menu bar options that are now valid */

//	save_as_item->set_sensitive (true);
	save_template_item->set_sensitive (true);
	snapshot_item->set_sensitive (true);
	save_item->set_sensitive (true);
	add_track_item->set_sensitive (true);
	export_item->set_sensitive (true);
	close_item->set_sensitive (true);
	locations_dialog_check->set_sensitive (true);
	route_params_check->set_sensitive (true);
	connection_editor_check->set_sensitive (true);

	cleanup_item->set_sensitive (true);

	/* sensitize transport bar */

	goto_start_button.set_sensitive (true);
	goto_end_button.set_sensitive (true);
	roll_button.set_sensitive (true);
	stop_button.set_sensitive (true);
	play_selection_button.set_sensitive (true);
	rec_button.set_sensitive (true);
	auto_loop_button.set_sensitive (true);
	shuttle_box.set_sensitive (true);
	
	/* <CMT Additions> */
	if (image_compositor_item) {
		image_compositor_item->set_sensitive(true) ;
	}
	/* </CMT Additions> */
	

	if (session->n_diskstreams()) {
		// meter_bridge_dialog_check->set_sensitive (true);
	} else {
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


	Blink.connect (mem_fun(*this, &ARDOUR_UI::transport_rec_enable_blink));
	Blink.connect (mem_fun(*this, &ARDOUR_UI::solo_blink));
	Blink.connect (mem_fun(*this, &ARDOUR_UI::audition_blink));

	/* these are all need to be handled in an RT-safe and MT way, so don't
	   do any GUI work, just queue it for handling by the GUI thread.
	*/

	session->TransportStateChange.connect (mem_fun(*this, &ARDOUR_UI::queue_transport_change));
	session->ControlChanged.connect (mem_fun(*this, &ARDOUR_UI::queue_map_control_change));

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
	
	map_button_state ();

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

	/* desensitize menu bar options that are now invalid */

//	save_as_item->set_sensitive (false);
	save_template_item->set_sensitive (false);
	snapshot_item->set_sensitive (false);
	save_item->set_sensitive (false);
	add_track_item->set_sensitive (false);
	export_item->set_sensitive (false);
	close_item->set_sensitive (false);
	// meter_bridge_dialog_check->set_sensitive (false);
	connection_editor_check->set_sensitive (false);
	locations_dialog_check->set_sensitive (false);
	// meter_bridge_dialog_check->set_active(false);
	connection_editor_check->set_active(false);
	locations_dialog_check->set_active(false);
	route_params_check->set_sensitive (false);

	/* desensitize transport bar */

	goto_start_button.set_sensitive (false);
	goto_end_button.set_sensitive (false);
	roll_button.set_sensitive (false);
	stop_button.set_sensitive (false);
	play_selection_button.set_sensitive (false);
	rec_button.set_sensitive (false);
	auto_loop_button.set_sensitive (false);
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
ARDOUR_UI::create_meter_bridge ()
{
	if (meter_bridge == 0) {
		meter_bridge = new MeterBridge ();
		meter_bridge->signal_unmap().connect (mem_fun(*this, &ARDOUR_UI::meter_bridge_hiding));
	}
	return 0;
}

void
ARDOUR_UI::meter_bridge_hiding()
{
	// meter_bridge_dialog_check->set_active(false);
}

int
ARDOUR_UI::create_connection_editor ()
{
	if (connection_editor == 0) {
		// GTK2FIX
		// connection_editor = new ConnectionEditor ();
		// connection_editor->signal_unmap().connect (mem_fun(*this, &ARDOUR_UI::connection_editor_hiding));
	}

	if (session) {
		// connection_editor->set_session (session);
	}

	return 0;
}

void
ARDOUR_UI::toggle_connection_editor ()
{
	if (create_connection_editor()) {
		return;
	}

	//GTK2FIX
#if 0

	if (connection_editor->within_hiding()) {
		return;
	}
						      

	if (connection_editor_check->get_active()){
		connection_editor->show_all();
	} else {
		connection_editor->hide_all();
	}
#endif
}

void
ARDOUR_UI::connection_editor_hiding()
{
	//GTK2FIX
	// connection_editor_check->set_active(false);
}

void
ARDOUR_UI::big_clock_hiding()
{
	big_clock_check->set_active(false);
}

void
ARDOUR_UI::toggle_big_clock_window ()
{
	if (big_clock_window->within_hiding()) {
		return;
	}

	if (big_clock_window->is_visible()) {
		big_clock_window->hide_all ();
	} else {
		big_clock_window->show_all ();
	}
}

void
ARDOUR_UI::toggle_options_window ()
{
	if (option_editor == 0) {
		option_editor = new OptionEditor (*this, *editor, *mixer);
		option_editor->signal_unmap().connect(mem_fun(*this, &ARDOUR_UI::option_hiding));
		option_editor->set_session (session);
	} else if (option_editor->within_hiding()) {
		return;
	}

	if (option_editor->is_visible()) {
		option_editor->hide_all ();
	} else {
		option_editor->show_all ();
	}
}

void
ARDOUR_UI::option_hiding ()
{
	options_window_check->set_active(false);
}

void
ARDOUR_UI::toggle_auto_input ()

{
	toggle_some_session_state (auto_input_button,
				   &Session::get_auto_input,
				   &Session::set_auto_input);
	
	meter_bridge->clear_all_meters ();
}

void
ARDOUR_UI::toggle_metering ()
{
#if 0
	if (global_meter_button.get_active()) {
		meter_bridge->toggle_metering ();
	}
#endif
}

int
ARDOUR_UI::create_location_ui ()
{
	if (location_ui == 0) {
		location_ui = new LocationUI ();
		location_ui->set_session (session);
		location_ui->signal_unmap().connect (mem_fun(*this, &ARDOUR_UI::location_ui_hiding));
	} 
	return 0;
}

void
ARDOUR_UI::toggle_location_window ()
{
	if (create_location_ui()) {
		return;
	}

	if (location_ui->within_hiding()) {
		return;
	}

	if (location_ui->is_visible()) {
		location_ui->hide_all();
	} else {
		location_ui->show_all();
	}
}

void
ARDOUR_UI::location_ui_hiding()
{
	locations_dialog_check->set_active(false);
}

int
ARDOUR_UI::create_route_params ()
{
	if (route_params == 0) {
		route_params = new RouteParams_UI (*engine);
		route_params->set_session (session);
		route_params->signal_unmap().connect (mem_fun(*this, &ARDOUR_UI::route_params_hiding));
	}
	return 0;
}

void
ARDOUR_UI::toggle_route_params_window ()
{
	if (create_route_params ()) {
		return;
	}

	if (route_params->within_hiding()) {
		return;
	}

	if (route_params->is_visible ()) {
		route_params->hide_all ();
	} else {
		route_params->show_all ();
	}
}
	
void
ARDOUR_UI::route_params_hiding ()
{
	route_params_check->set_active (false);
}

void
ARDOUR_UI::toggle_sound_file_browser ()
{
	if (sfdb_check->get_active()) {
		SoundFileBrowser sfdb(_("Sound File Browser"));

		sfdb_check->signal_toggled().connect (bind (mem_fun (sfdb, &Gtk::Dialog::response), Gtk::RESPONSE_CANCEL));
		sfdb.run();
		sfdb_check->set_active(false);
	}
}

