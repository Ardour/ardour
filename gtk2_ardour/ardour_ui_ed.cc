/*
    Copyright (C) 20002-2004 Paul Davis 

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

    $Id$
*/

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <pbd/pathscanner.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "editor.h"

#include <ardour/session.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

int
ARDOUR_UI::create_editor ()

{
	try {
		editor = new Editor (*engine);
	}

	catch (failed_constructor& err) {
		return -1;
	}

	editor->DisplayControlChanged.connect (mem_fun(*this, &ARDOUR_UI::editor_display_control_changed));

	return 0;
}

void
ARDOUR_UI::build_menu_bar ()

{
	using namespace Menu_Helpers;

	menu_bar.set_name ("MainMenuBar");

	MenuList& items = menu_bar.items();

	/* file menu */
	
	Menu *session_menu = manage (new Menu);
	MenuList& session_items = session_menu->items();
	session_menu->set_name ("ArdourContextMenu");

	session_items.push_back (MenuElem (_("New"), bind (mem_fun(*this, &ARDOUR_UI::new_session), false, string ())));
	session_items.push_back (MenuElem (_("Open"), mem_fun(*this, &ARDOUR_UI::open_session)));
	session_items.push_back (MenuElem (_("Recent"), mem_fun(*this, &ARDOUR_UI::open_recent_session)));
	session_items.push_back (MenuElem (_("Close"), mem_fun(*this, &ARDOUR_UI::close_session)));
	close_item = session_items.back();
	close_item->set_sensitive (false);

	session_items.push_back (SeparatorElem());

	session_items.push_back (MenuElem (_("Add Track/Bus"), mem_fun(*this, &ARDOUR_UI::add_route)));
	add_track_item = session_items.back ();
	add_track_item->set_sensitive (false);

	session_items.push_back (SeparatorElem());
	
	/* <CMT Additions> */

	PathScanner scanner;
	vector<string*>* results = scanner (getenv ("PATH"), "AniComp", false, false);

	if (results && !results->empty()) {
		Menu* image_compositor_menu = manage(new Menu());
		MenuList& image_compositor_items = image_compositor_menu->items();
		image_compositor_menu->set_name ("ArdourContextMenu");
		image_compositor_items.push_back(MenuElem (_("Connect"), (mem_fun (editor, &PublicEditor::connect_to_image_compositor)))) ;
		session_items.push_back(MenuElem (_("Image Compositor"), *image_compositor_menu)) ;
		image_compositor_item = session_items.back() ;
		image_compositor_item->set_sensitive(false) ;
		session_items.push_back (SeparatorElem());
	} else {
		image_compositor_item = 0;
	}

	if (results) {
		delete results;
	}

	/* </CMT Additions> */

	session_items.push_back (MenuElem (_("Save"), bind (mem_fun(*this, &ARDOUR_UI::save_state), string(""))));
	save_item = session_items.back();
	save_item->set_sensitive (false);

	session_items.push_back (MenuElem (_("Snapshot"), mem_fun(*this, &ARDOUR_UI::snapshot_session)));
	snapshot_item = session_items.back();
	snapshot_item->set_sensitive (false);
/*
	session_items.push_back (MenuElem (_("Save as...")));
	save_as_item = session_items.back();
	save_as_item->set_sensitive (false);
*/
	session_items.push_back (MenuElem (_("Save Template..."), mem_fun(*this, &ARDOUR_UI::save_template)));
	save_template_item = session_items.back();
	save_template_item->set_sensitive (false);

	Menu *export_menu = manage (new Menu);
	MenuList& export_items = export_menu->items();
	export_menu->set_name ("ArdourContextMenu");
	export_items.push_back (MenuElem (_("Export session to audiofile..."), mem_fun (*editor, &PublicEditor::export_session)));
	export_items.push_back (MenuElem (_("Export range to audiofile..."), mem_fun (*editor, &PublicEditor::export_selection)));
	// export_items.back()->set_sensitive (false);

	session_items.push_back (MenuElem (_("Export"), *export_menu));
	export_item = session_items.back();
	export_item->set_sensitive (false);

	session_items.push_back (SeparatorElem());

	Menu *cleanup_menu = manage (new Menu);
	MenuList& cleanup_items = cleanup_menu->items();
	cleanup_menu->set_name ("ArdourContextMenu");
	cleanup_items.push_back (MenuElem (_("Cleanup unused sources"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup)));
	cleanup_items.push_back (MenuElem (_("Flush wastebasket"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash)));

	session_items.push_back (MenuElem (_("Cleanup"), *cleanup_menu));
	cleanup_item = session_items.back ();
	cleanup_item->set_sensitive (false);

	session_items.push_back (SeparatorElem());

	session_items.push_back (MenuElem (_("Quit"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::finish)));

	items.push_back (MenuElem (_("Session"), *session_menu));

	/* edit menu; the editor is responsible for the contents */

	Menu *edit_menu = manage (new Menu);
	editor->set_edit_menu (*edit_menu);
	items.push_back (MenuElem (_("Edit"), *edit_menu));
	edit_menu->set_name ("ArdourContextMenu");
	
	/* JACK menu for controlling ... JACK */

	Menu* jack_menu = manage (new Menu);
	MenuList& jack_items = jack_menu->items();
	jack_menu->set_name ("ArdourContextMenu");
	
	jack_items.push_back (MenuElem (_("Disconnect"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::disconnect_from_jack)));
	jack_disconnect_item = jack_items.back();
	jack_disconnect_item->set_sensitive (false);
	jack_items.push_back (MenuElem (_("Reconnect"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::reconnect_to_jack)));
	jack_reconnect_item = jack_items.back();
	jack_reconnect_item->set_sensitive (false);

	jack_bufsize_menu = manage (new Menu);
	MenuList& jack_bufsize_items = jack_bufsize_menu->items();
	jack_bufsize_menu->set_name ("ArdourContextMenu");

	jack_bufsize_items.push_back (MenuElem (X_("32"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 32)));
	jack_bufsize_items.push_back (MenuElem (X_("64"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 64)));
	jack_bufsize_items.push_back (MenuElem (X_("128"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 128)));
	jack_bufsize_items.push_back (MenuElem (X_("256"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 256)));
	jack_bufsize_items.push_back (MenuElem (X_("512"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 512)));
	jack_bufsize_items.push_back (MenuElem (X_("1024"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 1024)));
	jack_bufsize_items.push_back (MenuElem (X_("2048"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 2048)));
	jack_bufsize_items.push_back (MenuElem (X_("4096"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 4096)));
	jack_bufsize_items.push_back (MenuElem (X_("8192"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (jack_nframes_t) 8192)));

	jack_items.push_back (MenuElem (_("Latency"), *jack_bufsize_menu));
	jack_bufsize_menu->set_sensitive (false);

	items.push_back (MenuElem (_("JACK"), *jack_menu));
	
	/* windows menu */

	Menu *window_menu = new Menu();
	MenuList& window_items = window_menu->items();
	window_menu->set_name ("ArdourContextMenu");
	
	window_items.push_back (TearoffMenuElem());

	window_items.push_back (MenuElem (_("Editor"), mem_fun(*this, &ARDOUR_UI::goto_editor_window)));
	window_items.push_back (MenuElem (_("Mixer"), mem_fun(*this, &ARDOUR_UI::goto_mixer_window)));

	window_items.push_back (SeparatorElem());

	window_items.push_back 
		(CheckMenuElem 
		 (_("Options Editor"),
		  mem_fun(*this, &ARDOUR_UI::toggle_options_window)));
	 options_window_check = dynamic_cast<CheckMenuItem*>(window_items.back());
	// options_window_check->set_sensitive (false);

	window_items.push_back
		(CheckMenuElem
		 (_("Audio Library"),
		  mem_fun(*this, &ARDOUR_UI::toggle_sfdb_window)));
	sfdb_check = dynamic_cast<CheckMenuItem*>(window_items.back());

	window_items.push_back 
		(CheckMenuElem 
		 (_("Track/Bus Inspector"),
		  mem_fun(*this, &ARDOUR_UI::toggle_route_params_window)));
	route_params_check = dynamic_cast<CheckMenuItem*>(window_items.back());
	route_params_check->set_sensitive (false);

	window_items.push_back 
		(CheckMenuElem 
		 (_("Connections"),
		  mem_fun(*this, &ARDOUR_UI::toggle_connection_editor)));
	connection_editor_check = dynamic_cast<CheckMenuItem*>(window_items.back());
	connection_editor_check->set_sensitive (false);

#if 0
	window_items.push_back 
		(CheckMenuElem 
		 (_("Meter Bridge"),
		  mem_fun(*this, &ARDOUR_UI::toggle_meter_bridge_window)));
	meter_bridge_dialog_check = dynamic_cast<CheckMenuItem*>(window_items.back());
	meter_bridge_dialog_check->set_sensitive (false);
#endif

	window_items.push_back 
		(CheckMenuElem 
		 (_("Locations"),
		  mem_fun(*this, &ARDOUR_UI::toggle_location_window)));
	locations_dialog_check = dynamic_cast<CheckMenuItem*>(window_items.back());
	locations_dialog_check->set_sensitive (false);

	window_items.push_back 
		(CheckMenuElem 
		 (_("Big Clock"),
		  mem_fun(*this, &ARDOUR_UI::toggle_big_clock_window)));
	big_clock_check = dynamic_cast<CheckMenuItem*>(window_items.back());

	window_items.push_back (SeparatorElem());

	window_items.push_back (MenuElem (_("About"), mem_fun(*this, &ARDOUR_UI::show_splash)));
	
	
	items.push_back (MenuElem (_("Windows"), *window_menu));

	wall_clock_box.add (wall_clock_label);
	wall_clock_box.set_name ("WallClock");
	wall_clock_label.set_name ("WallClock");

	disk_space_box.add (disk_space_label);
	disk_space_box.set_name ("WallClock");
	disk_space_label.set_name ("WallClock");

	cpu_load_box.add (cpu_load_label);
	cpu_load_box.set_name ("CPULoad");
	cpu_load_label.set_name ("CPULoad");

	buffer_load_box.add (buffer_load_label);
	buffer_load_box.set_name ("BufferLoad");
	buffer_load_label.set_name ("BufferLoad");

//	disk_rate_box.add (disk_rate_label);
//	disk_rate_box.set_name ("DiskRate");
//	disk_rate_label.set_name ("DiskRate");

	sample_rate_box.add (sample_rate_label);
	sample_rate_box.set_name ("SampleRate");
	sample_rate_label.set_name ("SampleRate");

	menu_hbox.pack_start (menu_bar, true, true);
	menu_hbox.pack_end (wall_clock_box, false, false, 10);
	menu_hbox.pack_end (disk_space_box, false, false, 10);
	menu_hbox.pack_end (cpu_load_box, false, false, 10);
//	menu_hbox.pack_end (disk_rate_box, false, false, 10);
	menu_hbox.pack_end (buffer_load_box, false, false, 10);
	menu_hbox.pack_end (sample_rate_box, false, false, 10);

	menu_bar_base.set_name ("MainMenuBar");
	menu_bar_base.add (menu_hbox);
}


void
ARDOUR_UI::editor_display_control_changed (Editing::DisplayControl c)
{
	switch (c) {
	case Editing::FollowPlayhead:
		follow_button.set_active (editor->follow_playhead ());
		break;
	default:
		break;
	}
}

