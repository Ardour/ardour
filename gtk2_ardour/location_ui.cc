/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <cmath>
#include <cstdlib>

#include <gtkmm2ext/utils.h>

#include "ardour/session.h"
#include "pbd/memento_command.h"
#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "clock_group.h"
#include "enums_convert.h"
#include "main_clock.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "location_ui.h"
#include "utils.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

LocationEditRow::LocationEditRow(Session * sess, Location * loc, int32_t num)
	: SessionHandlePtr (0) /* explicitly set below */
	, location(0)
	, item_table (1, 6, false)
	, start_clock (X_("locationstart"), true, "", true, false)
	, start_to_playhead_button (_("Use PH"))
	, locate_to_start_button (_("Goto"))
	, end_clock (X_("locationend"), true, "", true, false)
	, end_to_playhead_button (_("Use PH"))
	, locate_to_end_button (_("Goto"))
	, length_clock (X_("locationlength"), true, "", true, false, true)
	, cd_check_button (_("CD"))
	, hide_check_button (_("Hide"))
	, lock_check_button (_("Lock"))
	, glue_check_button (_("Glue"))
	, _clock_group (0)
{

	i_am_the_modifier = 0;

	remove_button.set_icon (ArdourIcon::CloseCross);
	remove_button.set_events (remove_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	number_label.set_name ("LocationEditNumberLabel");
	date_label.set_name ("LocationDateLabel");
	name_label.set_name ("LocationEditNameLabel");
	name_entry.set_name ("LocationEditNameEntry");
	cd_check_button.set_name ("LocationEditCdButton");
	hide_check_button.set_name ("LocationEditHideButton");
	lock_check_button.set_name ("LocationEditLockButton");
	glue_check_button.set_name ("LocationEditGlueButton");
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
	start_hbox.pack_start (locate_to_start_button, false, false);
	start_hbox.pack_start (start_clock, false, false);
	start_hbox.pack_start (start_to_playhead_button, false, false);

	/* this is always in this location, no matter what the location is */

	item_table.attach (remove_button, 8, 9, 0, 1, SHRINK, SHRINK, 4, 1);
	item_table.attach (start_hbox, 0, 1, 0, 1, FILL, Gtk::AttachOptions(0), 4, 0);

	start_to_playhead_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::to_playhead_button_pressed), LocStart));
	locate_to_start_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_button_pressed), LocStart));
	start_clock.ValueChanged.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::clock_changed), LocStart));
	start_clock.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_to_clock), &start_clock), false);

	end_hbox.set_spacing (2);
	end_hbox.pack_start (locate_to_end_button, false, false);
	end_hbox.pack_start (end_clock, false, false);
	end_hbox.pack_start (end_to_playhead_button, false, false);

	end_to_playhead_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::to_playhead_button_pressed), LocEnd));
	locate_to_end_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_button_pressed), LocEnd));
	end_clock.ValueChanged.connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::clock_changed), LocEnd));
	end_clock.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &LocationEditRow::locate_to_clock), &end_clock), false);

	length_clock.ValueChanged.connect (sigc::bind ( sigc::mem_fun(*this, &LocationEditRow::clock_changed), LocLength));

	cd_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::cd_toggled));
	hide_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::hide_toggled));
	lock_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::lock_toggled));
	glue_check_button.signal_toggled().connect(sigc::mem_fun(*this, &LocationEditRow::glue_toggled));

	remove_button.signal_clicked.connect(sigc::mem_fun(*this, &LocationEditRow::remove_button_pressed));

	pack_start(item_table, true, true);

	set_location (loc);
	set_number (num);
	cd_toggled(); // show/hide cd-track details
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
		item_table.attach (hide_check_button, 5, 6, 0, 1, FILL, Gtk::FILL, 4, 0);
		item_table.attach (lock_check_button, 6, 7, 0, 1, FILL, Gtk::FILL, 4, 0);
		item_table.attach (glue_check_button, 7, 8, 0, 1, FILL, Gtk::FILL, 4, 0);

		Glib::DateTime gdt(Glib::DateTime::create_now_local (location->timestamp()));
		string date = gdt.format ("%F %H:%M");
		date_label.set_text(date);
		item_table.attach (date_label, 9, 10, 0, 1, FILL, Gtk::FILL, 4, 0);
	}
	hide_check_button.set_active (location->is_hidden());
	lock_check_button.set_active (location->locked());
	glue_check_button.set_active (location->position_time_domain() == Temporal::BeatTime);

	if (location->is_auto_loop() || location-> is_auto_punch()) {
		// use label instead of entry

		name_label.set_text (location->name());
		name_label.set_size_request (80, -1);

		remove_button.hide ();

		if (!name_label.get_parent()) {
			item_table.attach (name_label, 2, 3, 0, 1, EXPAND|FILL, FILL, 4, 0);
		}

		name_label.show();

	} else {

		name_entry.set_text (location->name());
		name_entry.set_size_request (100, -1);
		name_entry.set_editable (true);
		name_entry.signal_changed().connect (sigc::mem_fun(*this, &LocationEditRow::name_entry_changed));

		if (!name_entry.get_parent()) {
			item_table.attach (name_entry, 2, 3, 0, 1, FILL | EXPAND, FILL, 4, 0);
		}
		name_entry.show();

		if (!cd_check_button.get_parent()) {
			item_table.attach (cd_check_button, 4, 5, 0, 1, FILL, Gtk::AttachOptions (0), 4, 0);
		}

		if (location->is_session_range()) {
			remove_button.set_sensitive (false);
		}

		cd_check_button.set_active (location->is_cd_marker());
		cd_check_button.show();

		hide_check_button.show();
		lock_check_button.show();
		glue_check_button.show();
	}

	start_clock.set (location->start(), true);


	if (!location->is_mark()) {
		if (!end_hbox.get_parent()) {
			item_table.attach (end_hbox, 1, 2, 0, 1, FILL, Gtk::AttachOptions (0), 4, 0);
		}
		if (!length_clock.get_parent()) {
			end_hbox.pack_start (length_clock, false, false, 4);
		}

		end_clock.set (location->end(), true);
		length_clock.set_duration (location->length(), true);

		end_clock.show();
		length_clock.show();

		if (location->is_cd_marker()) {
			show_cd_track_details ();
		}

		set_tooltip (&remove_button, _("Remove this range"));
		set_tooltip (start_clock, _("Start time - middle click to locate here"));
		set_tooltip (end_clock, _("End time - middle click to locate here"));
		set_tooltip (length_clock, _("Length"));

		set_tooltip (&start_to_playhead_button, _("Set range start from playhead location"));
		set_tooltip (&end_to_playhead_button, _("Set range end from playhead location"));

	} else {

		set_tooltip (&remove_button, _("Remove this marker"));
		set_tooltip (start_clock, _("Position - middle click to locate here"));

		set_tooltip (&start_to_playhead_button, _("Set marker time from playhead location"));

		end_clock.hide();
		length_clock.hide();
	}

	set_clock_editable_status ();

	--i_am_the_modifier;

	/* connect to per-location signals, since this row only cares about this location */

	location->NameChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::name_changed, this), gui_context());
	location->StartChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::start_changed, this), gui_context());
	location->EndChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::end_changed, this), gui_context());
	location->Changed.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::location_changed, this), gui_context());
	location->FlagsChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::flags_changed, this), gui_context());
	location->LockChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::lock_changed, this), gui_context());
	location->TimeDomainChanged.connect (connections, invalidator (*this), boost::bind (&LocationEditRow::time_domain_changed, this), gui_context());
}

void
LocationEditRow::name_entry_changed ()
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::name_entry_changed);

	if (i_am_the_modifier || !location) {
		return;
	}

	location->set_name (name_entry.get_text());
}


void
LocationEditRow::isrc_entry_changed ()
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::isrc_entry_changed);

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
	ENSURE_GUI_THREAD (*this, &LocationEditRow::performer_entry_changed);

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
	ENSURE_GUI_THREAD (*this, &LocationEditRow::composer_entry_changed);

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
			location->set_start (timepos_t (_session->transport_sample ()), false);
			break;
		case LocEnd:
			location->set_end (timepos_t (_session->transport_sample ()), false);
			if (location->is_session_range()) {
				_session->set_session_range_is_free (false);
			}
			break;
		default:
			break;
	}
}

void
LocationEditRow::locate_button_pressed (LocationPart part)
{
	switch (part) {
		case LocStart:
			_session->request_locate (start_clock.current_time().samples());
			break;
		case LocEnd:
			_session->request_locate (end_clock.current_time().samples());
			break;
		default:
			break;
	}
}

bool
LocationEditRow::locate_to_clock (GdkEventButton* ev, AudioClock* clock)
{
	if (Keyboard::is_button2_event (ev)) {
		_session->request_locate (clock->current_time().samples());
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
			location->set_start (start_clock.current_time(), false);
			break;
		case LocEnd:
			location->set_end (end_clock.current_time(), false);
			if (location->is_session_range()) {
				_session->set_session_range_is_free (false);
			}
			break;
		case LocLength:
			location->set_end (location->start() + length_clock.current_duration(), false);
			if (location->is_session_range()) {
				_session->set_session_range_is_free (false);
			}
		default:
			break;
	}
}

void
LocationEditRow::show_cd_track_details ()
{
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
}

void
LocationEditRow::cd_toggled ()
{
	if (i_am_the_modifier || !location) {
		return;
	}

	location->set_cd (cd_check_button.get_active(), this);

	if (location->is_cd_marker()) {

		show_cd_track_details ();

	} else if (cd_track_details_hbox.get_parent()){

		item_table.remove (cd_track_details_hbox);
		//	  item_table.resize(1, 7);
		redraw_ranges(); /* EMIT_SIGNAL */
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

	if (location->position_time_domain() == Temporal::AudioTime) {
		location->set_position_time_domain (Temporal::BeatTime);
	} else {
		location->set_position_time_domain (Temporal::AudioTime);
	}
}

void
LocationEditRow::remove_button_pressed ()
{
	if (!location) {
		return;
	}

	remove_requested (location); /* EMIT_SIGNAL */
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
LocationEditRow::end_changed ()
{
	ENSURE_GUI_THREAD (*this, &LocationEditRow::end_changed, loc)

	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	end_clock.set (location->end());
	length_clock.set_duration (location->length());

	i_am_the_modifier--;
}

void
LocationEditRow::start_changed ()
{
	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	start_clock.set (location->start());

	i_am_the_modifier--;
}

void
LocationEditRow::name_changed ()
{
	if (!location) return;

	// update end and length
	i_am_the_modifier++;

	name_entry.set_text(location->name());
	name_label.set_text(location->name());

	i_am_the_modifier--;

}

void
LocationEditRow::location_changed ()
{

	if (!location) return;

	i_am_the_modifier++;

	start_clock.set (location->start());
	end_clock.set (location->end());
	length_clock.set_duration (location->length());

	set_clock_editable_status ();

	i_am_the_modifier--;

}

void
LocationEditRow::flags_changed ()
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	cd_check_button.set_active (location->is_cd_marker());
	hide_check_button.set_active (location->is_hidden());
	glue_check_button.set_active (location->position_time_domain() == Temporal::BeatTime);

	i_am_the_modifier--;
}

void
LocationEditRow::lock_changed ()
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	lock_check_button.set_active (location->locked());

	set_clock_editable_status ();

	i_am_the_modifier--;
}

void
LocationEditRow::time_domain_changed ()
{
	if (!location) {
		return;
	}

	i_am_the_modifier++;

	glue_check_button.set_active (location->position_time_domain() == Temporal::BeatTime);

	i_am_the_modifier--;
}

void
LocationEditRow::focus_name()
{
	name_entry.grab_focus ();
}

void
LocationEditRow::set_clock_editable_status ()
{
	start_clock.set_editable (!location->locked());
	end_clock.set_editable (!location->locked());
	length_clock.set_editable (!location->locked());
}

/*------------------------------------------------------------------------*/

LocationUI::LocationUI (std::string state_node_name)
	: add_location_button (_("New Marker"))
	, add_range_button (_("New Range"))
	, _mode (AudioClock::Samples)
	, _mode_set (false)
	, _state_node_name (state_node_name)
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

	loop_punch_box.set_border_width (6); // 5 + 1 px framebox-border
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
	location_rows_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	location_rows_scroller.set_size_request (-1, 130);

	newest_location = 0;

	loc_frame_box.set_spacing (5);
	loc_frame_box.set_border_width (5);
	loc_frame_box.set_name("LocationFrameBox");

	loc_frame_box.pack_start (location_rows_scroller, true, true);

	add_location_button.set_name ("LocationAddLocationButton");

	table->attach (loc_frame_box, 0, 2, table_row, table_row + 1);
	++table_row;

	loc_range_panes.add (*table);

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
	range_rows_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	range_rows_scroller.set_size_request (-1, 130);

	range_frame_box.set_spacing (5);
	range_frame_box.set_name("LocationFrameBox");
	range_frame_box.set_border_width (5);
	range_frame_box.pack_start (range_rows_scroller, true, true);

	add_range_button.set_name ("LocationAddRangeButton");

	table->attach (range_frame_box, 0, 2, table_row, table_row + 1);
	++table_row;

	loc_range_panes.add (*table);

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
	loop_edit_row.unset_clock_group ();
	punch_edit_row.unset_clock_group ();
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

	PublicEditor::instance().begin_reversible_command (_("remove marker"));
	XMLNode &before = _session->locations()->get_state();
	_session->locations()->remove (loc);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	PublicEditor::instance().commit_reversible_command ();

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
	} else if (location->is_xrun()) {
		/* we don't show xrun markers here */
		return;
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

		if (location == newest_location) {
			newest_location = 0;
			erow->focus_name();
		}
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
LocationUI::map_locations (const Locations::LocationList& locations)
{
	Locations::LocationList::iterator i;
	gint n;
	int mark_n = 0;
	Locations::LocationList temp = locations;
	LocationSortByStart cmp;

	temp.sort (cmp);

	for (n = 0, i = temp.begin(); i != temp.end(); ++n, ++i) {

		Location* location = *i;

		if (location->is_mark()) {
			LocationEditRow* erow = manage (new LocationEditRow (_session, location, mark_n));

			erow->set_clock_group (*_clock_group);
			erow->remove_requested.connect (sigc::mem_fun(*this, &LocationUI::location_remove_requested));
			erow->redraw_ranges.connect (sigc::mem_fun(*this, &LocationUI::location_redraw_ranges));

			Box_Helpers::BoxList & loc_children = location_rows.children();
			loc_children.push_back(Box_Helpers::Element(*erow, PACK_SHRINK, 1, PACK_START));
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
		timepos_t where (_session->audible_sample());
		_session->locations()->next_available_name(markername,"mark");
		Location *location = new Location (*_session, where, where, markername, Location::IsMark);
		if (UIConfiguration::instance().get_name_new_markers()) {
			newest_location = location;
		}
		PublicEditor::instance().begin_reversible_command (_("add marker"));
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		PublicEditor::instance().commit_reversible_command ();
	}

}

void
LocationUI::add_new_range()
{
	string rangename;

	if (_session) {
		timepos_t where (_session->audible_sample());
		_session->locations()->next_available_name(rangename,"unnamed");
		Location *location = new Location (*_session, where, where, rangename, Location::IsRangeMarker);
		PublicEditor::instance().begin_reversible_command (_("add range marker"));
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		PublicEditor::instance().commit_reversible_command ();
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
		_session->locations()->added.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::location_added, this, _1), gui_context());
		_session->locations()->removed.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::location_removed, this, _1), gui_context());
		_session->locations()->changed.connect (_session_connections, invalidator (*this), boost::bind (&LocationUI::refresh_location_list, this), gui_context());

		_clock_group->set_clock_mode (clock_mode_from_session_instant_xml ());
	} else {
		_mode_set = false;
	}

	loop_edit_row.set_session (s);
	punch_edit_row.set_session (s);

	refresh_location_list ();
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

	_mode_set = false;

	SessionHandlePtr::session_going_away ();
}

XMLNode &
LocationUI::get_state () const
{
	XMLNode* node = new XMLNode (_state_node_name);
	node->set_property (X_("clock-mode"), _clock_group->clock_mode ());
	return *node;
}

int
LocationUI::set_state (const XMLNode& node)
{
	if (node.name() != _state_node_name) {
		return -1;
	}

	if (!node.get_property (X_("clock-mode"), _mode)) {
		return -1;
	}

	_mode_set = true;
	_clock_group->set_clock_mode (_mode);
	return 0;
}

AudioClock::Mode
LocationUI::clock_mode_from_session_instant_xml ()
{
	if (_mode_set) {
		return _mode;
	}

	XMLNode* node = _session->instant_xml (_state_node_name);
	if (!node) {
		return ARDOUR_UI::instance()->primary_clock->mode();
	}

	if (!node->get_property (X_("clock-mode"), _mode)) {
		return ARDOUR_UI::instance()->primary_clock->mode();
	}

	_mode_set = true;
	return _mode;
}


/*------------------------*/

LocationUIWindow::LocationUIWindow ()
	: ArdourWindow (S_("Ranges|Locations"))
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
	return false;
}

void
LocationUIWindow::set_session (Session *s)
{
	ArdourWindow::set_session (s);
	_ui.set_session (s);
	_ui.show_all ();
}

void
LocationUIWindow::session_going_away ()
{
	ArdourWindow::session_going_away ();
	hide_all();
}
