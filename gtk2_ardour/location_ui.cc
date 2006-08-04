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

    $Id$
*/

#include <cmath>
#include <cstdlib>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>

#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <pbd/memento_command.h>

#include "ardour_ui.h"
#include "prompter.h"
#include "location_ui.h"
#include "keyboard.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

LocationEditRow::LocationEditRow(Session * sess, Location * loc, int32_t num)
	: location(0), session(0),
	  item_table (1, 7, false),
	  start_set_button (_("Set")),
	  start_go_button (_("Go")),
	  start_clock (X_("LocationEditRowClock"), true),
	  end_set_button (_("Set")),
	  end_go_button (_("Go")),
	  end_clock (X_("LocationEditRowClock"), true),
	  length_clock (X_("LocationEditRowClock"), true, true),
	  cd_check_button (_("CD")),
	  hide_check_button (_("Hidden")),
	  remove_button (_("Remove")),
	  scms_check_button (_("SCMS")),
	  preemph_check_button (_("Pre-Emphasis"))

{
	
	i_am_the_modifier = 0;

	number_label.set_name ("LocationEditNumberLabel");
	name_label.set_name ("LocationEditNameLabel");
	name_entry.set_name ("LocationEditNameEntry");
	start_set_button.set_name ("LocationEditSetButton");
	start_go_button.set_name ("LocationEditGoButton");
	end_set_button.set_name ("LocationEditSetButton");
	end_go_button.set_name ("LocationEditGoButton");
	cd_check_button.set_name ("LocationEditCdButton");
	hide_check_button.set_name ("LocationEditHideButton");
	remove_button.set_name ("LocationEditRemoveButton");
	isrc_label.set_name ("LocationEditNumberLabel");
	isrc_entry.set_name ("LocationEditNameEntry");
	scms_check_button.set_name ("LocationEditCdButton");
	preemph_check_button.set_name ("LocationEditCdButton");
	performer_label.set_name ("LocationEditNumberLabel");
	performer_entry.set_name ("LocationEditNameEntry");
	composer_label.set_name ("LocationEditNumberLabel");
	composer_entry.set_name ("LocationEditNameEntry");


	isrc_label.set_text ("ISRC: ");
	isrc_label.set_size_request (30, -1);
	performer_label.set_text ("Performer: ");
	performer_label.set_size_request (60, -1);
	composer_label.set_text ("Composer: ");
	composer_label.set_size_request (60, -1);

	isrc_entry.set_size_request (112, -1);
	isrc_entry.set_max_length(12);
	isrc_entry.set_editable (true);

	performer_entry.set_size_request (100, -1);
	performer_entry.set_editable (true);

	composer_entry.set_size_request (100, -1);
	composer_entry.set_editable (true);

	cd_track_details_hbox.pack_start (isrc_label, false, false);
	cd_track_details_hbox.pack_start (isrc_entry, false, false);
	cd_track_details_hbox.pack_start (scms_check_button, false, false);
	cd_track_details_hbox.pack_start (preemph_check_button, false, false);
	cd_track_details_hbox.pack_start (performer_label, false, false);
	cd_track_details_hbox.pack_start (performer_entry, true, true);
	cd_track_details_hbox.pack_start (composer_label, false, false);
	cd_track_details_hbox.pack_start (composer_entry, true, true);

	isrc_entry.signal_changed().connect (mem_fun(*this, &LocationEditRow::isrc_entry_changed)); 
	performer_entry.signal_changed().connect (mem_fun(*this, &LocationEditRow::performer_entry_changed));
	composer_entry.signal_changed().connect (mem_fun(*this, &LocationEditRow::composer_entry_changed));
	scms_check_button.signal_toggled().connect(mem_fun(*this, &LocationEditRow::scms_toggled));
	preemph_check_button.signal_toggled().connect(mem_fun(*this, &LocationEditRow::preemph_toggled));


	set_session (sess);


	item_table.attach (number_label, 0, 1, 0, 1, FILL, FILL, 3, 0);
	
	start_hbox.pack_start (start_go_button, false, false);
	start_hbox.pack_start (start_clock, false, false);
	start_hbox.pack_start (start_set_button, false, false);

	item_table.attach (start_hbox, 2, 3, 0, 1, FILL, FILL, 4, 0);

	
	start_set_button.signal_clicked().connect(bind (mem_fun (*this, &LocationEditRow::set_button_pressed), LocStart));
	start_go_button.signal_clicked().connect(bind (mem_fun (*this, &LocationEditRow::go_button_pressed), LocStart));
 	start_clock.ValueChanged.connect (bind (mem_fun (*this, &LocationEditRow::clock_changed), LocStart));

	
	end_hbox.pack_start (end_go_button, false, false);
	end_hbox.pack_start (end_clock, false, false);
	end_hbox.pack_start (end_set_button, false, false);
	
	//item_table.attach (end_hbox, 2, 3, 0, 1, 0, 0, 4, 0);
	
	end_set_button.signal_clicked().connect(bind (mem_fun (*this, &LocationEditRow::set_button_pressed), LocEnd));
	end_go_button.signal_clicked().connect(bind (mem_fun (*this, &LocationEditRow::go_button_pressed), LocEnd));
	end_clock.ValueChanged.connect (bind (mem_fun (*this, &LocationEditRow::clock_changed), LocEnd));
	
//	item_table.attach (length_clock, 3, 4, 0, 1, 0, 0, 4, 0);
	length_clock.ValueChanged.connect (bind ( mem_fun(*this, &LocationEditRow::clock_changed), LocLength));

//	item_table.attach (cd_check_button, 4, 5, 0, 1, 0, Gtk::FILL, 4, 0);
//	item_table.attach (hide_check_button, 5, 6, 0, 1, 0, Gtk::FILL, 4, 0);
//	item_table.attach (remove_button, 7, 8, 0, 1, 0, Gtk::FILL, 4, 0);
	
	cd_check_button.signal_toggled().connect(mem_fun(*this, &LocationEditRow::cd_toggled));
	hide_check_button.signal_toggled().connect(mem_fun(*this, &LocationEditRow::hide_toggled));
	
	remove_button.signal_clicked().connect(mem_fun(*this, &LocationEditRow::remove_button_pressed));

	pack_start(item_table, true, true);

	set_location (loc);
	set_number (num);
}

LocationEditRow::~LocationEditRow()
{
	if (location) {
 		start_changed_connection.disconnect();
 		end_changed_connection.disconnect();
 		name_changed_connection.disconnect();
 		changed_connection.disconnect();
 		flags_changed_connection.disconnect();
	}
}

void
LocationEditRow::set_session (Session *sess)
{
	session = sess;

	if (!session) return;

	start_clock.set_session (session);
	end_clock.set_session (session);
	length_clock.set_session (session);	
	
}

void
LocationEditRow::set_number (int num)
{
	number = num;

	if (number >= 0 ) {
		number_label.set_text (string_compose ("%1", number));
	}
}

void
LocationEditRow::set_location (Location *loc)
{
	if (location) {
		start_changed_connection.disconnect();
		end_changed_connection.disconnect();
		name_changed_connection.disconnect();
		changed_connection.disconnect();
		flags_changed_connection.disconnect();
	}

	location = loc;

	if (!location) return;

	if (!hide_check_button.get_parent()) {
		item_table.attach (hide_check_button, 6, 7, 0, 1, FILL, Gtk::FILL, 4, 0);
	}
	hide_check_button.set_active (location->is_hidden());
	
	if (location->is_auto_loop() || location->is_auto_punch()) {
		// use label instead of entry

		name_label.set_text (location->name());
		name_label.set_size_request (80, -1);

		if (!name_label.get_parent()) {
			item_table.attach (name_label, 1, 2, 0, 1, FILL, Gtk::FILL, 4, 0);
		}
		
		name_label.show();

	} else {

		name_entry.set_text (location->name());
		name_entry.set_size_request (100, -1);
		name_entry.set_editable (true);
		name_entry.signal_changed().connect (mem_fun(*this, &LocationEditRow::name_entry_changed));  

		if (!name_entry.get_parent()) {
			item_table.attach (name_entry, 1, 2, 0, 1, FILL | EXPAND, FILL, 4, 0);
		}
		name_entry.show();

		if (!cd_check_button.get_parent()) {
			item_table.attach (cd_check_button, 5, 6, 0, 1, FILL, Gtk::FILL, 4, 0);
		}
		if (!remove_button.get_parent()) {
			item_table.attach (remove_button, 7, 8, 0, 1, FILL, Gtk::FILL, 4, 0);
		}

		/* XXX i can't find a way to hide the button without messing up 
		   the row spacing, so make it insensitive (paul).
		*/

		if (location->is_end()) {
			remove_button.set_sensitive (false);
		}

		cd_check_button.set_active (location->is_cd_marker());
		cd_check_button.show();
		hide_check_button.show();
	}

	start_clock.set (location->start(), true);
	

	if (!location->is_mark()) {
		if (!end_hbox.get_parent()) {
			item_table.attach (end_hbox, 3, 4, 0, 1, FILL, FILL, 4, 0);
		}
		if (!length_clock.get_parent()) {
			item_table.attach (length_clock, 4, 5, 0, 1, FILL, FILL, 4, 0);
		}

		end_clock.set (location->end(), true);
		length_clock.set (location->length(), true);

		end_set_button.show();
		end_go_button.show();
		end_clock.show();
		length_clock.show();
	}
	else {
		end_set_button.hide();
		end_go_button.hide();
		end_clock.hide();
		length_clock.hide();
	}

	start_changed_connection = location->start_changed.connect (mem_fun(*this, &LocationEditRow::start_changed));
	end_changed_connection = location->end_changed.connect (mem_fun(*this, &LocationEditRow::end_changed));
	name_changed_connection = location->name_changed.connect (mem_fun(*this, &LocationEditRow::name_changed));
	changed_connection = location->changed.connect (mem_fun(*this, &LocationEditRow::location_changed));
	flags_changed_connection = location->FlagsChanged.connect (mem_fun(*this, &LocationEditRow::flags_changed));
	
}

void
LocationEditRow::name_entry_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationEditRow::name_entry_changed));
	if (i_am_the_modifier || !location) return;

	location->set_name (name_entry.get_text());
}


void
LocationEditRow::isrc_entry_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationEditRow::isrc_entry_changed));
	
	if (i_am_the_modifier || !location) return;

	if (isrc_entry.get_text() != "" ) {

	  location->cd_info["isrc"] = isrc_entry.get_text();
	  
	} else {
	  location->cd_info.erase("isrc");
	}
}

void
LocationEditRow::performer_entry_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationEditRow::performer_entry_changed));
	
	if (i_am_the_modifier || !location) return;

	if (performer_entry.get_text() != "") {
	  location->cd_info["performer"] = performer_entry.get_text();
	} else {
	  location->cd_info.erase("performer");
	}
}

void
LocationEditRow::composer_entry_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationEditRow::composer_entry_changed));
	
	if (i_am_the_modifier || !location) return;

	if (composer_entry.get_text() != "") {
	location->cd_info["composer"] = composer_entry.get_text();
	} else {
	  location->cd_info.erase("composer");
	}
}


void
LocationEditRow::set_button_pressed (LocationPart part)
{
	if (!location) return;
	
	switch (part) {
	case LocStart:
		location->set_start (session->transport_frame ());
		break;
	case LocEnd:
		location->set_end (session->transport_frame ());
		break;
	default:
		break;
	}
}

void
LocationEditRow::go_button_pressed (LocationPart part)
{
	if (!location) return;

	switch (part) {
	case LocStart:
		ARDOUR_UI::instance()->do_transport_locate (location->start());
		break;
	case LocEnd:
		ARDOUR_UI::instance()->do_transport_locate (location->end());
		break;
	default:
		break;
	}
}

void
LocationEditRow::clock_changed (LocationPart part)
{
	if (i_am_the_modifier || !location) return;
	
	switch (part) {
	case LocStart:
		location->set_start (start_clock.current_time());
		break;
	case LocEnd:
		location->set_end (end_clock.current_time());
		break;
	case LocLength:
		location->set_end (location->start() + length_clock.current_duration());
	default:
		break;
	}

}

void
LocationEditRow::cd_toggled ()
{

	if (i_am_the_modifier || !location) return;
	location->set_cd (cd_check_button.get_active(), this);

	if (location->is_cd_marker() && !(location->is_mark())) {

	  if (location->cd_info.find("isrc") != location->cd_info.end()) {
	    isrc_entry.set_text(location->cd_info["isrc"]);
	  }
	  if (location->cd_info.find("performer") != location->cd_info.end()) {
	    performer_entry.set_text(location->cd_info["performer"]);
	  }
	  if (location->cd_info.find("composer") != location->cd_info.end()) {
	    composer_entry.set_text(location->cd_info["composer"]);
	  }
	  if (location->cd_info.find("scms") != location->cd_info.end()) {
	    scms_check_button.set_active(true);
	  }
	  if (location->cd_info.find("preemph") != location->cd_info.end()) {
	    preemph_check_button.set_active(true);
	  }
	  
	  if(!cd_track_details_hbox.get_parent()) {
	    item_table.attach (cd_track_details_hbox, 1, 8, 1, 2, FILL | EXPAND, FILL, 4, 0);
	  }
	  // item_table.resize(2, 7);
	  cd_track_details_hbox.show_all();

	} else if (cd_track_details_hbox.get_parent()){

	    item_table.remove (cd_track_details_hbox);	
	    //	  item_table.resize(1, 7);
	    redraw_ranges(); /* 	EMIT_SIGNAL */
	}

}


void
LocationEditRow::hide_toggled ()
{
	if (i_am_the_modifier || !location) return;

	location->set_hidden (hide_check_button.get_active(), this);
}

void
LocationEditRow::remove_button_pressed ()
{
	if (!location) return;

	remove_requested(location); /* 	EMIT_SIGNAL */
}



void
LocationEditRow::scms_toggled ()
{
	if (i_am_the_modifier || !location) return;

	if (scms_check_button.get_active()) {
	  location->cd_info["scms"] = "on";
	} else {
	  location->cd_info.erase("scms");
	}
	
}

void
LocationEditRow::preemph_toggled ()
{
	if (i_am_the_modifier || !location) return;

	if (preemph_check_button.get_active()) {
	  location->cd_info["preemph"] = "on";
	} else {
	  location->cd_info.erase("preemph");
	}
}

void
LocationEditRow::end_changed (ARDOUR::Location *loc)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationEditRow::end_changed), loc));

	if (!location) return;
		
	// update end and length
	i_am_the_modifier++;

	end_clock.set (location->end());
	length_clock.set (location->length());
	
	i_am_the_modifier--;
}

void
LocationEditRow::start_changed (ARDOUR::Location *loc)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationEditRow::start_changed), loc));
	
	if (!location) return;
	
	// update end and length
	i_am_the_modifier++;

	start_clock.set (location->start());
	
	i_am_the_modifier--;
}

void
LocationEditRow::name_changed (ARDOUR::Location *loc)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationEditRow::name_changed), loc));
	
	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	name_entry.set_text(location->name());
	name_label.set_text(location->name());

	i_am_the_modifier--;

}

void
LocationEditRow::location_changed (ARDOUR::Location *loc)
{	
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationEditRow::location_changed), loc));
	
	if (!location) return;

	i_am_the_modifier++;

	start_clock.set (location->start());
	end_clock.set (location->end());
	length_clock.set (location->length());

	i_am_the_modifier--;

}

void
LocationEditRow::flags_changed (ARDOUR::Location *loc, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationEditRow::flags_changed), loc, src));
	
	if (!location) return;

	i_am_the_modifier++;

	cd_check_button.set_active (location->is_cd_marker());
	hide_check_button.set_active (location->is_hidden());

	i_am_the_modifier--;
}

LocationUI::LocationUI ()
	: ArdourDialog ("location dialog"),
	  add_location_button (_("Add New Location")),
	  add_range_button (_("Add New Range"))
{
	i_am_the_modifier = 0;

	set_title(_("ardour: locations"));
	set_wmclass(_("ardour_locations"), "Ardour");

	set_name ("LocationWindow");

	get_vbox()->pack_start (location_hpacker);

	location_vpacker.set_border_width (10);
	location_vpacker.set_spacing (5);

	location_vpacker.pack_start (loop_edit_row, false, false);
	location_vpacker.pack_start (punch_edit_row, false, false);

	location_rows.set_name("LocationLocRows");
	location_rows_scroller.add (location_rows);
	location_rows_scroller.set_name ("LocationLocRowsScroller");
	location_rows_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	location_rows_scroller.set_size_request (-1, 130);

	loc_frame_box.set_spacing (5);
	loc_frame_box.set_border_width (5);
	loc_frame_box.set_name("LocationFrameBox");
	
	loc_frame_box.pack_start (location_rows_scroller, true, true);

	add_location_button.set_name ("LocationAddLocationButton");
	loc_frame_box.pack_start (add_location_button, false, false);

	loc_frame.set_name ("LocationLocEditorFrame");
	loc_frame.set_label (_("Location (CD Index) Markers"));
	loc_frame.add (loc_frame_box);
	loc_range_panes.pack1(loc_frame, true, false);

	
	range_rows.set_name("LocationRangeRows");
	range_rows_scroller.add (range_rows);
	range_rows_scroller.set_name ("LocationRangeRowsScroller");
	range_rows_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	range_rows_scroller.set_size_request (-1, 130);
	
	range_frame_box.set_spacing (5);
	range_frame_box.set_name("LocationFrameBox");
	range_frame_box.set_border_width (5);
	range_frame_box.pack_start (range_rows_scroller, true, true);

	add_range_button.set_name ("LocationAddRangeButton");
	range_frame_box.pack_start (add_range_button, false, false);

	range_frame.set_name ("LocationRangeEditorFrame");
	range_frame.set_label (_("Range (CD Track) Markers"));
	range_frame.add (range_frame_box);
	loc_range_panes.pack2(range_frame, true, false);
	location_vpacker.pack_start (loc_range_panes, true, true);
	
	location_hpacker.pack_start (location_vpacker, true, true);

	add_location_button.signal_clicked().connect (mem_fun(*this, &LocationUI::add_new_location));
	add_range_button.signal_clicked().connect (mem_fun(*this, &LocationUI::add_new_range));
	
	//add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_RELEASE_MASK);

	
}

LocationUI::~LocationUI()
{
}



gint LocationUI::do_location_remove (ARDOUR::Location *loc)
{
	/* this is handled internally by Locations, but there's
	   no point saving state etc. when we know the marker
	   cannot be removed.
	*/

	if (loc->is_end()) {
		return FALSE;
	}

	session->begin_reversible_command (_("remove marker"));
	XMLNode &before = session->locations()->get_state();
	session->locations()->remove (loc);
	XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), before, after));
	session->commit_reversible_command ();

	return FALSE;
}

void LocationUI::location_remove_requested (ARDOUR::Location *loc)
{
	// must do this to prevent problems when destroying
	// the effective sender of this event
	
  Glib::signal_idle().connect (bind (mem_fun(*this, &LocationUI::do_location_remove), loc));
}


void LocationUI::location_redraw_ranges ()
{

	range_rows.hide();
	range_rows.show();

}


void
LocationUI::location_added (Location* location)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationUI::location_added), location));
	
	if (location->is_auto_punch()) {
		punch_edit_row.set_location(location);
	}
	else if (location->is_auto_loop()) {
		loop_edit_row.set_location(location);
	}
	else {
		refresh_location_list ();
	}
}

void
LocationUI::location_removed (Location* location)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationUI::location_removed), location));
	
	if (location->is_auto_punch()) {
		punch_edit_row.set_location(0);
	}
	else if (location->is_auto_loop()) {
		loop_edit_row.set_location(0);
	}
	else {
		refresh_location_list ();
	}
}

struct LocationSortByStart {
    bool operator() (Location *a, Location *b) {
	    return a->start() < b->start();
    }
};

void
LocationUI::map_locations (Locations::LocationList& locations)
{
	Locations::LocationList::iterator i;
	Location* location;
	gint n; 
	int mark_n = 0;
	Locations::LocationList temp = locations;
	LocationSortByStart cmp;

	temp.sort (cmp);
	locations = temp;

	Box_Helpers::BoxList & loc_children = location_rows.children();
	Box_Helpers::BoxList & range_children = range_rows.children();
	LocationEditRow * erow;
	
	for (n = 0, i = locations.begin(); i != locations.end(); ++n, ++i) {

		location = *i;

		if (location->is_mark()) {
			mark_n++;
			erow = manage (new LocationEditRow(session, location, mark_n));
			erow->remove_requested.connect (mem_fun(*this, &LocationUI::location_remove_requested));
 			erow->redraw_ranges.connect (mem_fun(*this, &LocationUI::location_redraw_ranges));
			loc_children.push_back(Box_Helpers::Element(*erow, PACK_SHRINK, 1, PACK_START));
		}
		else if (location->is_auto_punch()) {
			punch_edit_row.set_session (session);
			punch_edit_row.set_location (location);
		}
		else if (location->is_auto_loop()) {
			loop_edit_row.set_session (session);
			loop_edit_row.set_location (location);
		}
		else {
			erow = manage (new LocationEditRow(session, location));
			erow->remove_requested.connect (mem_fun(*this, &LocationUI::location_remove_requested));
			range_children.push_back(Box_Helpers::Element(*erow,  PACK_SHRINK, 1, PACK_START));
		}
	}

	range_rows.show_all();
	location_rows.show_all();
}

void
LocationUI::add_new_location()
{
	if (session) {
		jack_nframes_t where = session->audible_frame();
		Location *location = new Location (where, where, "mark", Location::IsMark);
		session->begin_reversible_command (_("add marker"));
		XMLNode &before = session->locations()->get_state();
		session->locations()->add (location, true);
		XMLNode &after = session->locations()->get_state();
		session->add_command (new MementoCommand<Locations>(*(session->locations()), before, after));
		session->commit_reversible_command ();
	}
	
}

void
LocationUI::add_new_range()
{
	if (session) {
		jack_nframes_t where = session->audible_frame();
		Location *location = new Location (where, where, "unnamed", 
											Location::IsRangeMarker);
		session->begin_reversible_command (_("add range marker"));
		XMLNode &before = session->locations()->get_state();
		session->locations()->add (location, true);
		XMLNode &after = session->locations()->get_state();
		session->add_command (new MementoCommand<Locations>(*(session->locations()), before, after));
		session->commit_reversible_command ();
	}
}


void
LocationUI::refresh_location_list_s (Change ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LocationUI::refresh_location_list_s), ignored));
	
	refresh_location_list ();
}

void
LocationUI::refresh_location_list ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationUI::refresh_location_list));
	using namespace Box_Helpers;

	BoxList & loc_children = location_rows.children();
	BoxList & range_children = range_rows.children();

	loc_children.clear();
	range_children.clear();

	if (session) {
		session->locations()->apply (*this, &LocationUI::map_locations);
	}
	
}

void
LocationUI::set_session(ARDOUR::Session* sess)
{
	ArdourDialog::set_session (sess);

	if (session) {
		session->locations()->changed.connect (mem_fun(*this, &LocationUI::refresh_location_list));
		session->locations()->StateChanged.connect (mem_fun(*this, &LocationUI::refresh_location_list_s));
		session->locations()->added.connect (mem_fun(*this, &LocationUI::location_added));
		session->locations()->removed.connect (mem_fun(*this, &LocationUI::location_removed));
		session->going_away.connect (mem_fun(*this, &LocationUI::session_gone));
	}
	refresh_location_list ();
}

void
LocationUI::session_gone()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &LocationUI::session_gone));
	
	hide_all();

	using namespace Box_Helpers;
	BoxList & loc_children = location_rows.children();
	BoxList & range_children = range_rows.children();

	loc_children.clear();
	range_children.clear();

	loop_edit_row.set_session (0);
	loop_edit_row.set_location (0);

	punch_edit_row.set_session (0);
	punch_edit_row.set_location (0);

	ArdourDialog::session_gone ();
}

bool
LocationUI::on_delete_event (GdkEventAny* ev)
{
	hide ();
	return true;
}
