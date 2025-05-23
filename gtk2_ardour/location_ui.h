/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <ytkmm/box.h>
#include <ytkmm/checkbutton.h>
#include <ytkmm/button.h>
#include <ytkmm/table.h>
#include <ytkmm/entry.h>
#include <ytkmm/label.h>
#include <ytkmm/scrolledwindow.h>

#include "pbd/signals.h"

#include "ardour/location.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/pane.h"

#include "ardour_window.h"
#include "audio_clock.h"

namespace ARDOUR {
	class Location;
}

class ClockGroup;

class LocationEditRow  : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	LocationEditRow (ARDOUR::Session *sess=0, ARDOUR::Location *loc=0, int32_t num=-1);
	virtual ~LocationEditRow();

	void set_location (ARDOUR::Location*);
	ARDOUR::Location * get_location() { return location; }

	void set_session (ARDOUR::Session *);

	void set_number (int);
	void focus_name();
	void set_clock_group (ClockGroup&);
	void unset_clock_group () { _clock_group = 0; }

	sigc::signal<void,ARDOUR::Location*> remove_requested;
	sigc::signal<void> redraw_ranges;

protected:

	enum LocationPart {
		LocStart,
		LocEnd,
		LocLength
	};

	ARDOUR::Location *location;

	Gtk::Table    item_table;

	Gtk::Entry    name_entry;
	Gtk::Label    name_label;
	Gtk::Label    number_label;
	Gtk::Label    date_label;

	Gtk::HBox     start_hbox;
	AudioClock    start_clock;
	ArdourWidgets::ArdourButton start_to_playhead_button;
	ArdourWidgets::ArdourButton locate_to_start_button;

	Gtk::HBox     end_hbox;
	AudioClock    end_clock;
	ArdourWidgets::ArdourButton end_to_playhead_button;
	ArdourWidgets::ArdourButton locate_to_end_button;

	AudioClock    length_clock;
	Gtk::CheckButton cd_check_button;
	Gtk::CheckButton section_check_button;
	Gtk::CheckButton hide_check_button;
	Gtk::CheckButton lock_check_button;

	ArdourWidgets::ArdourButton remove_button;

	Gtk::HBox     cd_track_details_hbox;
	Gtk::Entry    isrc_entry;
	Gtk::Label    isrc_label;


	Gtk::Label    performer_label;
	Gtk::Entry    performer_entry;
	Gtk::Label    composer_label;
	Gtk::Entry    composer_entry;
	Gtk::CheckButton   scms_check_button;
	Gtk::Label         scms_label;
	Gtk::CheckButton   preemph_check_button;
	Gtk::Label         preemph_label;
	ClockGroup* _clock_group;

	guint32 i_am_the_modifier;
	int   number;

	void name_entry_changed ();
	void isrc_entry_changed ();
	void performer_entry_changed ();
	void composer_entry_changed ();

	void to_playhead_button_pressed (LocationPart part);
	void locate_button_pressed (LocationPart part);

	void clock_changed (LocationPart part);
	bool locate_to_clock (GdkEventButton*, AudioClock*);

	void cd_toggled ();
	void section_toggled ();
	void hide_toggled ();
	void lock_toggled ();
	void remove_button_pressed ();

	void scms_toggled ();
	void preemph_toggled ();

	void end_changed ();
	void start_changed ();
	void name_changed ();
	void location_changed ();
	void flags_changed ();
	void lock_changed ();

	void set_clock_editable_status ();
	void show_cd_track_details ();

	PBD::ScopedConnectionList connections;
};

class LocationUI : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	LocationUI (std::string state_node_name = "LocationUI");
	~LocationUI ();

	void set_session (ARDOUR::Session *);

	void add_new_location();
	void add_new_range();

	void refresh_location_list ();

	XMLNode & get_state () const;
	int set_state (const XMLNode&);

private:
	/** set to the location that has just been created with the LocationUI `add' button
	    (if Config->get_name_new_markers() is true); if it is non-0, the name entry of
	    the location is given the focus by location_added().
	*/
	ARDOUR::Location *newest_location;

	void session_going_away ();

	LocationEditRow      loop_edit_row;
	LocationEditRow      punch_edit_row;
	Gtk::VBox loop_punch_box;

	ArdourWidgets::VPane loc_range_panes;

	Gtk::VBox  loc_frame_box;
	Gtk::Button add_location_button;
	Gtk::ScrolledWindow  location_rows_scroller;
	Gtk::VBox            location_rows;

	Gtk::VBox  range_frame_box;
	Gtk::Button add_range_button;
	Gtk::ScrolledWindow  range_rows_scroller;
	Gtk::VBox            range_rows;

	/* When any location changes it start
	   or end points, it sends a signal that is caught
	   by one of these functions
	*/

	void location_remove_requested (ARDOUR::Location *);

	void location_redraw_ranges ();

	void start_changed (ARDOUR::Location*);

	gint do_location_remove (ARDOUR::Location *);

	guint32 i_am_the_modifier;

	void location_removed (ARDOUR::Location *);
	void location_added (ARDOUR::Location *);
	void map_locations (const ARDOUR::Locations::LocationList&);

	ClockGroup* _clock_group;
	AudioClock::Mode clock_mode_from_session_instant_xml ();

	AudioClock::Mode _mode;
	bool _mode_set;

	std::string _state_node_name;
};

class LocationUIWindow : public ArdourWindow
{
public:
	LocationUIWindow ();
	~LocationUIWindow ();

	void on_map ();
	void set_session (ARDOUR::Session *);

	LocationUI& ui() { return _ui; }

protected:
	LocationUI _ui;
	bool on_delete_event (GdkEventAny*);
	void session_going_away();
};

