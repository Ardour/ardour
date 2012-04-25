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

#include <cmath>
#include <cstdlib>

#include <gtkmm2ext/utils.h>

#include "ardour/utils.h"
#include "ardour/configuration.h"
#include "ardour/session.h"
#include "pbd/memento_command.h"

#include "ardour_ui.h"
#include "clock_group.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "location_ui.h"
#include "prompter.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

LocationEditRow::LocationEditRow(Session * sess, Location * loc, int32_t num)
	: SessionHandlePtr (0) /* explicitly set below */
        , location(0)
        , item_table (1, 6, false)
        , start_clock (X_("locationstart"), true, "", true, false)
	, start_to_playhead_button (_("Use PH"))
        , end_clock (X_("locationend"), true, "", true, false)
	, end_to_playhead_button (_("Use PH"))
        , length_clock (X_("locationlength"), true, "", true, false, true)
        , cd_check_button (_("CD"))
        , hide_check_button (_("Hide"))
        , lock_check_button (_("Lock"))
        , glue_check_button (_("Glue"))
        , _clock_group (0)
{
         i_am_the_modifier = 0;

         remove_button.set_image (*manage (new Image (Stock::REMOVE, Gtk::ICON_SIZE_MENU)));

         number_label.set_name ("LocationEditNumberLabel");
         name_label.set_name ("LocationEditNameLabel");
         name_entry.set_name ("LocationEditNameEntry");
         cd_check_button.set_name ("LocationEditCdButton");
         hide_check_button.set_name ("LocationEditHideButton");
         lock_check_button.set_name ("LocationEditLockButton");
         glue_check_button.set_name ("LocationEditGlueButton");
         remove_button.set_name ("LocationEditRemoveButton");
         isrc_label.set_name ("LocationEditNumberLabel");
         isrc_entry.set_name ("LocationEditNameEntry");
         scms_check_button.set_name ("LocationEditCdButton");
         preemph_check_button.set_name ("LocationEditCdButton");
         performer_label.set_name ("LocationEditNumberLabel");
         performer_entry.set_name ("LocationEditNameEntry");
         composer_label.set_name ("LocationEditNumberLabel");
         composer_entry.set_name ("LocationEditNameEntry");

         isrc_label.set_text (X_("ISRC:"));
         performer_label.set_text (_("Performer:"));
         composer_label.set_text (_("Composer:"));
	 scms_label.set_text (X_("SCMS"));
	 preemph_label.set_text (_("Pre-Emphasis"));

         isrc_entry.set_size_request (112, -1);
         isrc_entry.set_max_length(12);
         isrc_entry.set_editable (true);

         performer_entry.set_size_request (100, -1);
         performer_entry.set_editable (true);

         composer_entry.set_size_request (100, -1);
         composer_entry.set_editable (true);

         name_label.set_alignment (0, 0.5);

	 Gtk::HBox* front_spacing = manage (new HBox);
	 front_spacing->set_size_request (20, -1);
	 Gtk::HBox* mid_spacing = manage (new HBox);
	 mid_spacing->set_size_request (20, -1);

	 cd_track_details_hbox.set_spacing (4);
	 cd_track_details_hbox.pack_start (*front_spacing, false, false);
         cd_track_details_hbox.pack_start (isrc_label, false, false);
         cd_track_details_hbox.pack_start (isrc_entry, false, false);
         cd_track_details_hbox.pack_start (performer_label, false, false);
         cd_track_details_hbox.pack_start (performer_entry, true, true);
         cd_track_details_hbox.pack_start (composer_label, false, false);
         cd_track_details_hbox.pack_start (composer_entry, true, true);
	 cd_track_details_hbox.pack_start (*mid_spacing, false, false);
         cd_track_details_hbox.pack_start (scms_label, false, false);
         cd_track_details_hbox.pack_start (scms_check_button, false, false);
         cd_track_details_hbox.pack_start (preemph_label, false, false);
         cd_track_details_hbox.pack_start (preemph_check_button, false, false);

         isrc_entry.signal_changed().connect (sigc::mem_fun(*this, &LocationEditRow::isrc_entry_changed));
         performer_entry.signal_changed().connect (sigc::mem_fun(*this, &LocationEditRow::performer_entry_changed));
         composer_entry.signal_changed().connect (sigc::mem_fun(*this, &LocationEditRow::composer_entry_changed));
         scms_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::scms_toggled));
         preemph_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::preemph_toggled));

         set_session (sess);

	 start_hbox.set_spacing (2);
         start_hbox.pack_start (start_clock, false, false);
	 start_hbox.pack_start (start_to_playhead_button, false, false);

         /* this is always in this location, no matter what the location is */

	 VBox *rbox = manage (new VBox);
	 rbox->pack_start (remove_button, false, false);

	 item_table.attach (*rbox, 0, 1, 0, 1, FILL, Gtk::AttachOptions (0), 4, 0);
         item_table.attach (start_hbox, 2, 3, 0, 1, FILL, Gtk::AttachOptions(0), 4, 0);

	 start_to_playhead_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::to_playhead_button_pressed), LocStart));
         start_clock.ValueChanged.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::clock_changed), LocStart));
	 start_clock.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_to_clock), &start_clock), false);

	 end_hbox.set_spacing (2);
         end_hbox.pack_start (end_clock, false, false);
	 end_hbox.pack_start (end_to_playhead_button, false, false);

	 end_to_playhead_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::to_playhead_button_pressed), LocEnd));
         end_clock.ValueChanged.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::clock_changed), LocEnd));
	 end_clock.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_to_clock), &end_clock), false);

         length_clock.ValueChanged.connect (sigc::bind ( sigc::mem_fun(*this, &LocationEditRow::clock_changed), LocLength));

         cd_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::cd_toggled));
         hide_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::hide_toggled));
         lock_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::lock_toggled));
         glue_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::glue_toggled));

         remove_button.signal_clicked().connect(sigc::mem_fun(*this, &LocationEditRow::remove_button_pressed));

         pack_start(item_table, true, true);

         set_location (loc);
         set_number (num);
 }

 LocationEditRow::~LocationEditRow()
 {
         if (location) {
                 connections.drop_connections ();
         }

         if (_clock_group) {
                 _clock_group->remove (start_clock);
                 _clock_group->remove (end_clock);
                 _clock_group->remove (length_clock);
         }
 }

 void
 LocationEditRow::set_clock_group (ClockGroup& cg)
 {
         if (_clock_group) {
                 _clock_group->remove (start_clock);
                 _clock_group->remove (end_clock);
                 _clock_group->remove (length_clock);
         }

         _clock_group = &cg;

         _clock_group->add (start_clock);
         _clock_group->add (end_clock);
         _clock_group->add (length_clock);
}

void
LocationEditRow::set_session (Session *sess)
{
	SessionHandlePtr::set_session (sess);

	if (!_session) {
		return;
	}

	start_clock.set_session (_session);
	end_clock.set_session (_session);
	length_clock.set_session (_session);
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
		connections.drop_connections ();
	}

	location = loc;

	if (!location) {
		return;
	}

	++i_am_the_modifier;

	if (!hide_check_button.get_parent()) {
		item_table.attach (hide_check_button, 6, 7, 0, 1, FILL, Gtk::FILL, 4, 0);
		item_table.attach (lock_check_button, 7, 8, 0, 1, FILL, Gtk::FILL, 4, 0);
		item_table.attach (glue_check_button, 8, 9, 0, 1, FILL, Gtk::FILL, 4, 0);
	}
	hide_check_button.set_active (location->is_hidden());
	lock_check_button.set_active (location->locked());
	glue_check_button.set_active (location->position_lock_style() == MusicTime);

	if (location->is_auto_loop() || location-> is_auto_punch()) {
		// use label instead of entry

		name_label.set_text (location->name());
		name_label.set_size_request (80, -1);

		remove_button.hide ();

		if (!name_label.get_parent()) {
			item_table.attach (name_label, 1, 2, 0, 1, FILL, FILL, 4, 0);
		}

		name_label.show();

	} else {

		name_entry.set_text (location->name());
		name_entry.set_size_request (100, -1);
		name_entry.set_editable (true);
		name_entry.signal_changed().connect (sigc::mem_fun(*this, &LocationEditRow::name_entry_changed));

		if (!name_entry.get_parent()) {
			item_table.attach (name_entry, 1, 2, 0, 1, FILL | EXPAND, FILL, 4, 0);
		}
		name_entry.show();

		if (!cd_check_button.get_parent()) {
			item_table.attach (cd_check_button, 5, 6, 0, 1, FILL, Gtk::AttachOptions (0), 4, 0);
		}

		if (location->is_session_range()) {
			remove_button.set_sensitive (false);
		}

		cd_check_button.set_active (location->is_cd_marker());
		cd_check_button.show();

		if (location->start() == _session->current_start_frame()) {
			cd_check_button.set_sensitive (false);
		} else {
			cd_check_button.set_sensitive (true);
		}

		hide_check_button.show();
		lock_check_button.show();
		glue_check_button.show();
	}

	start_clock.set (location->start(), true);


	if (!location->is_mark()) {
		if (!end_hbox.get_parent()) {
			item_table.attach (end_hbox, 3, 4, 0, 1, FILL, Gtk::AttachOptions (0), 4, 0);
		}
		if (!length_clock.get_parent()) {
			end_hbox.pack_start (length_clock, false, false);
		}

		end_clock.set (location->end(), true);
		length_clock.set (location->length(), true);

		end_clock.show();
		length_clock.show();

		ARDOUR_UI::instance()->set_tip (remove_button, _("Remove this range"));
		ARDOUR_UI::instance()->set_tip (start_clock, _("Start time - middle click to locate here"));
		ARDOUR_UI::instance()->set_tip (end_clock, _("End time - middle click to locate here"));
		ARDOUR_UI::instance()->set_tip (length_clock, _("Length"));

		ARDOUR_UI::instance()->tooltips().set_tip (start_to_playhead_button, _("Set range start from playhead location"));
		ARDOUR_UI::instance()->tooltips().set_tip (end_to_playhead_button, _("Set range end from playhead location"));
		
	} else {

		ARDOUR_UI::instance()->set_tip (remove_button, _("Remove this marker"));
		ARDOUR_UI::instance()->set_tip (start_clock, _("Position - middle click to locate here"));

		ARDOUR_UI::instance()->tooltips().set_tip (start_to_playhead_button, _("Set marker time from playhead location"));

		end_clock.hide();
		length_clock.hide();
	}

	set_clock_sensitivity ();

	--i_am_the_modifier;

	location->start_changed.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::start_changed, this, _1), gui_context());
	location->end_changed.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::end_changed, this, _1), gui_context());
	location->name_changed.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::name_changed, this, _1), gui_context());
	location->changed.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::location_changed, this, _1), gui_context());
	location->FlagsChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::flags_changed, this, _1, _2), gui_context());
	location->LockChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::lock_changed, this, _1), gui_context());
	location->PositionLockStyleChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::position_lock_style_changed, this, _1), gui_context());
}

void
LocationEditRow::name_entry_changed ()
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::name_entry_changed)

	if (i_am_the_modifier || !location) {
		return;
	}

	location->set_name (name_entry.get_text());
}


void
LocationEditRow::isrc_entry_changed ()
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::isrc_entry_changed)

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
	ENSURE_GUI_THREAD (*this, &LocationEditRow::performer_entry_changed)

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
	ENSURE_GUI_THREAD (*this, &LocationEditRow::composer_entry_changed)

	if (i_am_the_modifier || !location) return;

	if (composer_entry.get_text() != "") {
	location->cd_info["composer"] = composer_entry.get_text();
	} else {
	  location->cd_info.erase("composer");
	}
}

void
LocationEditRow::to_playhead_button_pressed (LocationPart part)
{
	if (!location) {
		return;
	}

	switch (part) {
	case LocStart:
		location->set_start (_session->transport_frame ());
		break;
	case LocEnd:
		location->set_end (_session->transport_frame ());
		break;
	default:
		break;
	}
}

bool
LocationEditRow::locate_to_clock (GdkEventButton* ev, AudioClock* clock)
{
	if (Keyboard::is_button2_event (ev)) {
		_session->request_locate (clock->current_time());
		return true;
	}
	return false;
}

void
LocationEditRow::clock_changed (LocationPart part)
{
	if (i_am_the_modifier || !location) {
		return;
	}

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
	if (i_am_the_modifier || !location) {
		return;
	}

	//if (cd_check_button.get_active() == location->is_cd_marker()) {
	//	return;
	//}

	if (cd_check_button.get_active()) {
		if (location->start() <= _session->current_start_frame()) {
			error << _("You cannot put a CD marker at the start of the session") << endmsg;
			cd_check_button.set_active (false);
			return;
		}
	}

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

		if (!cd_track_details_hbox.get_parent()) {
			item_table.attach (cd_track_details_hbox, 0, 7, 1, 2, FILL | EXPAND, FILL, 4, 0);
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
	if (i_am_the_modifier || !location) {
		return;
	}

	location->set_hidden (hide_check_button.get_active(), this);
}

void
LocationEditRow::lock_toggled ()
{
	if (i_am_the_modifier || !location) {
		return;
	}

	if (location->locked()) {
		location->unlock ();
	} else {
		location->lock ();
	}
}

void
LocationEditRow::glue_toggled ()
{
	if (i_am_the_modifier || !location) {
		return;
	}

	if (location->position_lock_style() == AudioTime) {
		location->set_position_lock_style (MusicTime);
	} else {
		location->set_position_lock_style (AudioTime);
	}
}

void
LocationEditRow::remove_button_pressed ()
{
	if (!location) {
		return;
	}

	remove_requested (location); /*	EMIT_SIGNAL */
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
LocationEditRow::end_changed (ARDOUR::Location *)
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::end_changed, loc)

	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	end_clock.set (location->end());
	length_clock.set (location->length());

	i_am_the_modifier--;
}

void
LocationEditRow::start_changed (ARDOUR::Location*)
{
	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	start_clock.set (location->start());

	if (location->start() == _session->current_start_frame()) {
		cd_check_button.set_sensitive (false);
	} else {
		cd_check_button.set_sensitive (true);
	}

	i_am_the_modifier--;
}

void
LocationEditRow::name_changed (ARDOUR::Location *)
{
	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	name_entry.set_text(location->name());
	name_label.set_text(location->name());

	i_am_the_modifier--;

}

void
LocationEditRow::location_changed (ARDOUR::Location*)
{

	if (!location) return;

	i_am_the_modifier++;

	start_clock.set (location->start());
	end_clock.set (location->end());
	length_clock.set (location->length());

	set_clock_sensitivity ();

	i_am_the_modifier--;

}

void
LocationEditRow::flags_changed (ARDOUR::Location*, void *)
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	cd_check_button.set_active (location->is_cd_marker());
	hide_check_button.set_active (location->is_hidden());
	glue_check_button.set_active (location->position_lock_style() == MusicTime);

	i_am_the_modifier--;
}

void
LocationEditRow::lock_changed (ARDOUR::Location*)
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	lock_check_button.set_active (location->locked());

	set_clock_sensitivity ();

	i_am_the_modifier--;
}

void
LocationEditRow::position_lock_style_changed (ARDOUR::Location*)
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	glue_check_button.set_active (location->position_lock_style() == MusicTime);

	i_am_the_modifier--;
}

void
LocationEditRow::focus_name() {
	name_entry.grab_focus();
}

void
LocationEditRow::set_clock_sensitivity ()
{
	start_clock.set_sensitive (!location->locked());
	end_clock.set_sensitive (!location->locked());
	length_clock.set_sensitive (!location->locked());
}

/*------------------------------------------------------------------------*/

LocationUI::LocationUI ()
	: add_location_button (_("New Marker"))
	, add_range_button (_("New Range"))
{
	i_am_the_modifier = 0;

        _clock_group = new ClockGroup;

	VBox* vbox = manage (new VBox);

	Table* table = manage (new Table (2, 2));
	table->set_spacings (2);
	table->set_col_spacing (0, 32);
	int table_row = 0;

	Label* l = manage (new Label (_("<b>Loop/Punch Ranges</b>")));
	l->set_alignment (0, 0.5);
	l->set_use_markup (true);
	table->attach (*l, 0, 2, table_row, table_row + 1);
	++table_row;

        loop_edit_row.set_clock_group (*_clock_group);
        punch_edit_row.set_clock_group (*_clock_group);

	loop_punch_box.pack_start (loop_edit_row, false, false);
	loop_punch_box.pack_start (punch_edit_row, false, false);

	table->attach (loop_punch_box, 1, 2, table_row, table_row + 1);
	++table_row;

	vbox->pack_start (*table, false, false);

 	table = manage (new Table (3, 2));
	table->set_spacings (2);
	table->set_col_spacing (0, 32);
	table_row = 0;

	table->attach (*manage (new Label ("")), 0, 2, table_row, table_row + 1, Gtk::SHRINK, Gtk::SHRINK);
	++table_row;

	l = manage (new Label (_("<b>Markers (Including CD Index)</b>")));
	l->set_alignment (0, 0.5);
	l->set_use_markup (true);
	table->attach (*l, 0, 2, table_row, table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
	++table_row;

	location_rows.set_name("LocationLocRows");
	location_rows_scroller.add (location_rows);
	location_rows_scroller.set_name ("LocationLocRowsScroller");
	location_rows_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	location_rows_scroller.set_size_request (-1, 130);

	newest_location = 0;

	loc_frame_box.set_spacing (5);
	loc_frame_box.set_border_width (5);
	loc_frame_box.set_name("LocationFrameBox");

	loc_frame_box.pack_start (location_rows_scroller, true, true);

	add_location_button.set_name ("LocationAddLocationButton");

	table->attach (loc_frame_box, 0, 2, table_row, table_row + 1);
	++table_row;

	loc_range_panes.pack1 (*table, true, false);

 	table = manage (new Table (3, 2));
	table->set_spacings (2);
	table->set_col_spacing (0, 32);
	table_row = 0;

	table->attach (*manage (new Label ("")), 0, 2, table_row, table_row + 1, Gtk::SHRINK, Gtk::SHRINK);
	++table_row;

	l = manage (new Label (_("<b>Ranges (Including CD Track Ranges)</b>")));
	l->set_alignment (0, 0.5);
	l->set_use_markup (true);
	table->attach (*l, 0, 2, table_row, table_row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
	++table_row;

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

	table->attach (range_frame_box, 0, 2, table_row, table_row + 1);
	++table_row;

	loc_range_panes.pack2 (*table, true, false);

	HBox* add_button_box = manage (new HBox);
	add_button_box->pack_start (add_location_button, true, true);
	add_button_box->pack_start (add_range_button, true, true);

	vbox->pack_start (loc_range_panes, true, true);
	vbox->pack_start (*add_button_box, false, false);

	pack_start (*vbox);

	add_location_button.signal_clicked().connect (sigc::mem_fun(*this, &LocationUI::add_new_location));
	add_range_button.signal_clicked().connect (sigc::mem_fun(*this, &LocationUI::add_new_range));

	show_all ();

	signal_map().connect (sigc::mem_fun (*this, &LocationUI::refresh_location_list));
}

LocationUI::~LocationUI()
{
        delete _clock_group;
}

gint
LocationUI::do_location_remove (ARDOUR::Location *loc)
{
	/* this is handled internally by Locations, but there's
	   no point saving state etc. when we know the marker
	   cannot be removed.
	*/

	if (loc->is_session_range()) {
		return FALSE;
	}

	_session->begin_reversible_command (_("remove marker"));
	XMLNode &before = _session->locations()->get_state();
	_session->locations()->remove (loc);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();

	return FALSE;
}

void
LocationUI::location_remove_requested (ARDOUR::Location *loc)
{
	// must do this to prevent problems when destroying
	// the effective sender of this event

	Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &LocationUI::do_location_remove), loc));
}


void
LocationUI::location_redraw_ranges ()
{
	range_rows.hide();
	range_rows.show();
}

struct LocationSortByStart {
    bool operator() (Location *a, Location *b) {
	    return a->start() < b->start();
    }
};

void
LocationUI::location_added (Location* location)
{
	if (location->is_auto_punch()) {
		punch_edit_row.set_location(location);
	} else if (location->is_auto_loop()) {
		loop_edit_row.set_location(location);
	} else if (location->is_range_marker() || location->is_mark()) {
		Locations::LocationList loc = _session->locations()->list ();
		loc.sort (LocationSortByStart ());

		LocationEditRow* erow = manage (new LocationEditRow (_session, location));

                erow->set_clock_group (*_clock_group);
		erow->remove_requested.connect (sigc::mem_fun (*this, &LocationUI::location_remove_requested));

		Box_Helpers::BoxList & children = location->is_range_marker() ? range_rows.children () : location_rows.children ();

		/* Step through the location list and the GUI list to find the place to insert */
		Locations::LocationList::iterator i = loc.begin ();
		Box_Helpers::BoxList::iterator j = children.begin ();
		while (i != loc.end()) {

			if (location->flags() != (*i)->flags()) {
				/* Skip locations in the session list that aren't of the right type */
				++i;
				continue;
			}

			if (*i == location) {
				children.insert (j, Box_Helpers::Element (*erow, PACK_SHRINK, 1, PACK_START));
				break;
			}

			++i;

			if (j != children.end()) {
				++j;
			}
		}

		range_rows.show_all ();
		location_rows.show_all ();
	}
}

void
LocationUI::location_removed (Location* location)
{
	ENSURE_GUI_THREAD (*this, &LocationUI::location_removed, location)

	if (location->is_auto_punch()) {
		punch_edit_row.set_location(0);
	} else if (location->is_auto_loop()) {
		loop_edit_row.set_location(0);
	} else if (location->is_range_marker() || location->is_mark()) {
		Box_Helpers::BoxList& children = location->is_range_marker() ? range_rows.children () : location_rows.children ();
		for (Box_Helpers::BoxList::iterator i = children.begin(); i != children.end(); ++i) {
			LocationEditRow* r = dynamic_cast<LocationEditRow*> (i->get_widget());
			if (r && r->get_location() == location) {
				children.erase (i);
				break;
			}
		}
	}
}

void
LocationUI::map_locations (Locations::LocationList& locations)
{
	Locations::LocationList::iterator i;
	gint n;
	int mark_n = 0;
	Locations::LocationList temp = locations;
	LocationSortByStart cmp;

	temp.sort (cmp);
	locations = temp;

	for (n = 0, i = locations.begin(); i != locations.end(); ++n, ++i) {

		Location* location = *i;

		if (location->is_mark()) {
			LocationEditRow* erow = manage (new LocationEditRow (_session, location, mark_n));

			erow->set_clock_group (*_clock_group);
			erow->remove_requested.connect (sigc::mem_fun(*this, &LocationUI::location_remove_requested));
			erow->redraw_ranges.connect (sigc::mem_fun(*this, &LocationUI::location_redraw_ranges));

                        Box_Helpers::BoxList & loc_children = location_rows.children();
			loc_children.push_back(Box_Helpers::Element(*erow, PACK_SHRINK, 1, PACK_START));
			if (location == newest_location) {
				newest_location = 0;
				erow->focus_name();
			}
		} else if (location->is_auto_punch()) {
			punch_edit_row.set_session (_session);
			punch_edit_row.set_location (location);
			punch_edit_row.show_all();
		} else if (location->is_auto_loop()) {
			loop_edit_row.set_session (_session);
			loop_edit_row.set_location (location);
			loop_edit_row.show_all();
		} else {
			LocationEditRow* erow = manage (new LocationEditRow(_session, location));

                        erow->set_clock_group (*_clock_group);
			erow->remove_requested.connect (sigc::mem_fun(*this, &LocationUI::location_remove_requested));

			Box_Helpers::BoxList & range_children = range_rows.children();
			range_children.push_back(Box_Helpers::Element(*erow,  PACK_SHRINK, 1, PACK_START));
		}
	}

	range_rows.show_all();
	location_rows.show_all();
}

void
LocationUI::add_new_location()
{
	string markername;

	if (_session) {
		framepos_t where = _session->audible_frame();
		_session->locations()->next_available_name(markername,"mark");
		Location *location = new Location (*_session, where, where, markername, Location::IsMark);
		if (Config->get_name_new_markers()) {
			newest_location = location;
		}
		_session->begin_reversible_command (_("add marker"));
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		_session->commit_reversible_command ();
	}

}

void
LocationUI::add_new_range()
{
	string rangename;

	if (_session) {
		framepos_t where = _session->audible_frame();
		_session->locations()->next_available_name(rangename,"unnamed");
		Location *location = new Location (*_session, where, where, rangename, Location::IsRangeMarker);
		_session->begin_reversible_command (_("add range marker"));
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		_session->commit_reversible_command ();
	}
}

void
LocationUI::refresh_location_list ()
{
	ENSURE_GUI_THREAD (*this, &LocationUI::refresh_location_list)
	using namespace Box_Helpers;

	// this is just too expensive to do when window is not shown
	if (!is_mapped()) {
		return;
	}

	BoxList & loc_children = location_rows.children();
	BoxList & range_children = range_rows.children();

	loc_children.clear();
	range_children.clear();

	if (_session) {
		_session->locations()->apply (*this, &LocationUI::map_locations);
	}
}

void
LocationUI::set_session(ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		_session->locations()->changed.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::locations_changed, this, _1), gui_context());
		_session->locations()->StateChanged.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::refresh_location_list, this), gui_context());
		_session->locations()->added.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::location_added, this, _1), gui_context());
		_session->locations()->removed.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::location_removed, this, _1), gui_context());
		_clock_group->set_clock_mode (clock_mode_from_session_instant_xml ());
	}

	loop_edit_row.set_session (s);
	punch_edit_row.set_session (s);

	refresh_location_list ();
}

void
LocationUI::locations_changed (Locations::Change c)
{
	/* removal is signalled by both a removed and a changed signal emission from Locations,
	   so we don't need to refresh the list on a removal
	*/
	if (c != Locations::REMOVAL) {
		refresh_location_list ();
	}
}

void
LocationUI::session_going_away()
{
	ENSURE_GUI_THREAD (*this, &LocationUI::session_going_away);

	using namespace Box_Helpers;
	BoxList & loc_children = location_rows.children();
	BoxList & range_children = range_rows.children();

	loc_children.clear();
	range_children.clear();

	loop_edit_row.set_session (0);
	loop_edit_row.set_location (0);

	punch_edit_row.set_session (0);
	punch_edit_row.set_location (0);

	SessionHandlePtr::session_going_away ();
}

XMLNode &
LocationUI::get_state () const
{
	XMLNode* node = new XMLNode (X_("LocationUI"));
	node->add_property (X_("clock-mode"), enum_2_string (_clock_group->clock_mode ()));
	return *node;
}

AudioClock::Mode
LocationUI::clock_mode_from_session_instant_xml () const
{
	XMLNode* node = _session->instant_xml (X_("LocationUI"));
	if (!node) {
		return AudioClock::Frames;
	}

	XMLProperty* p = node->property (X_("clock-mode"));
	if (!p) {
		return AudioClock::Frames;
	}
	      
	return (AudioClock::Mode) string_2_enum (p->value (), AudioClock::Mode);
}


/*------------------------*/

LocationUIWindow::LocationUIWindow ()
	: ArdourWindow (_("Locations"))
{
	set_wmclass(X_("ardour_locations"), PROGRAM_NAME);
	set_name ("LocationWindow");

	add (_ui);
}

LocationUIWindow::~LocationUIWindow()
{
}

void
LocationUIWindow::on_map ()
{
	ArdourWindow::on_map ();
	_ui.refresh_location_list();
}

bool
LocationUIWindow::on_delete_event (GdkEventAny*)
{
	hide ();
	return true;
}

void
LocationUIWindow::set_session (Session *s)
{
	ArdourWindow::set_session (s);
	_ui.set_session (s);
}

void
LocationUIWindow::session_going_away ()
{
	ArdourWindow::session_going_away ();
	hide_all();
}
