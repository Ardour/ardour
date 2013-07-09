/*
    Copyright (C) 1999-2002 Paul Davis

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

#ifndef __ardour_location_ui_h__
#define __ardour_location_ui_h__

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>

#include "pbd/signals.h"

#include "ardour/location.h"
#include "ardour/session_handle.h"

#include "ardour_window.h"
#include "audio_clock.h"

namespace ARDOUR {
	class LocationStack;
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

	Gtk::HBox     start_hbox;
	AudioClock    start_clock;
	Gtk::Button   start_to_playhead_button;

	Gtk::HBox     end_hbox;
	AudioClock    end_clock;
	Gtk::Button   end_to_playhead_button;

	AudioClock    length_clock;
	Gtk::CheckButton cd_check_button;
	Gtk::CheckButton hide_check_button;
	Gtk::CheckButton lock_check_button;
	Gtk::CheckButton glue_check_button;

	Gtk::Button   remove_button;

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

	void clock_changed (LocationPart part);
	bool locate_to_clock (GdkEventButton*, AudioClock*);

	void cd_toggled ();
	void hide_toggled ();
	void lock_toggled ();
	void glue_toggled ();
	void remove_button_pressed ();

	void scms_toggled ();
	void preemph_toggled ();

	void end_changed (ARDOUR::Location *);
	void start_changed (ARDOUR::Location *);
	void name_changed (ARDOUR::Location *);
	void location_changed (ARDOUR::Location *);
	void flags_changed (ARDOUR::Location *, void *src);
	void lock_changed (ARDOUR::Location *);
	void position_lock_style_changed (ARDOUR::Location *);

	void set_clock_editable_status ();
	void show_cd_track_details ();

	PBD::ScopedConnectionList connections;
};

class LocationUI : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
  public:
	LocationUI ();
	~LocationUI ();

	void set_session (ARDOUR::Session *);
        void set_clock_mode (AudioClock::Mode);

	void add_new_location();
	void add_new_range();

	void refresh_location_list ();

	XMLNode & get_state () const;

  private:
	ARDOUR::LocationStack* locations;
	/** set to the location that has just been created with the LocationUI `add' button
	    (if Config->get_name_new_markers() is true); if it is non-0, the name entry of
	    the location is given the focus by location_added().
	*/
	ARDOUR::Location *newest_location;

	void session_going_away ();

	LocationEditRow      loop_edit_row;
	LocationEditRow      punch_edit_row;
	Gtk::VBox loop_punch_box;

	Gtk::VPaned loc_range_panes;

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

	gint do_location_remove (ARDOUR::Location *);

	guint32 i_am_the_modifier;

	void location_removed (ARDOUR::Location *);
	void location_added (ARDOUR::Location *);
	void locations_changed (ARDOUR::Locations::Change);
	void map_locations (ARDOUR::Locations::LocationList&);

        ClockGroup* _clock_group;
	AudioClock::Mode clock_mode_from_session_instant_xml () const;
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

#endif // __ardour_location_ui_h__
