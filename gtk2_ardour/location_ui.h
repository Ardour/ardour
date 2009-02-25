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

#include "ardour/location.h"
#include "ardour/session.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class LocationStack;
	class Session;
	class Location;
}

class LocationEditRow  : public Gtk::HBox
{
  public:
	LocationEditRow (ARDOUR::Session *sess=0, ARDOUR::Location *loc=0, int32_t num=-1);
	virtual ~LocationEditRow();

	void set_location (ARDOUR::Location*);
	ARDOUR::Location * get_location() { return location; }

	void set_session (ARDOUR::Session *);

	void set_number (int);
	void focus_name();
	
	sigc::signal<void,ARDOUR::Location*> remove_requested;  
	sigc::signal<void> redraw_ranges;  

  protected:

	enum LocationPart {
		LocStart,
		LocEnd,
		LocLength
	};

	ARDOUR::Location *location;
	ARDOUR::Session  *session;

	
	
	Gtk::Table    item_table;
	
	Gtk::Entry    name_entry;
	Gtk::Label    name_label;
	Gtk::Label    number_label;
	
	Gtk::HBox     start_hbox;
	Gtk::Button   start_set_button;
	Gtk::Button   start_go_button;
	AudioClock    start_clock;

	Gtk::HBox     end_hbox;
	Gtk::Button   end_set_button;
	Gtk::Button   end_go_button;
	AudioClock    end_clock;

	AudioClock    length_clock;
	Gtk::CheckButton cd_check_button;
	Gtk::CheckButton hide_check_button;

	Gtk::Button   remove_button;

	Gtk::HBox     cd_track_details_hbox;
	Gtk::Entry    isrc_entry;
	Gtk::Label    isrc_label;


	Gtk::Label    performer_label;
	Gtk::Entry  performer_entry;
	Gtk::Label    composer_label;
 	Gtk::Entry  composer_entry;
	Gtk::CheckButton   scms_check_button;
	Gtk::CheckButton   preemph_check_button;


	guint32 i_am_the_modifier;
	int   number;
	
	void name_entry_changed ();
	void isrc_entry_changed ();
	void performer_entry_changed ();
	void composer_entry_changed ();
	       
	void set_button_pressed (LocationPart part);
	void go_button_pressed (LocationPart part);

	void clock_changed (LocationPart part);
	void change_aborted (LocationPart part);

	void cd_toggled ();
	void hide_toggled ();
	void remove_button_pressed ();

	void scms_toggled ();
	void preemph_toggled ();

	void end_changed (ARDOUR::Location *);
	void start_changed (ARDOUR::Location *);
	void name_changed (ARDOUR::Location *);
	void location_changed (ARDOUR::Location *);
	void flags_changed (ARDOUR::Location *, void *src);
	
	sigc::connection start_changed_connection;
	sigc::connection end_changed_connection;
	sigc::connection name_changed_connection;
	sigc::connection changed_connection;
	sigc::connection flags_changed_connection;
	
};


class LocationUI : public ArdourDialog
{
  public:
	LocationUI ();
	~LocationUI ();

	void set_session (ARDOUR::Session *);

	void on_show();

  private:
	ARDOUR::LocationStack* locations;
	ARDOUR::Location *newest_location;
        
	void session_gone();

	Gtk::VBox  location_vpacker;
	Gtk::HBox  location_hpacker;

	LocationEditRow      loop_edit_row;
	LocationEditRow      punch_edit_row;
	
	Gtk::VPaned loc_range_panes;
	
	Gtk::Frame loc_frame;
	Gtk::VBox  loc_frame_box;
	Gtk::Button add_location_button;
	Gtk::ScrolledWindow  location_rows_scroller;
	Gtk::VBox            location_rows;

	Gtk::Frame range_frame;
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

	void add_new_location();
	void add_new_range();
	
	void refresh_location_list ();
	void refresh_location_list_s (ARDOUR::Change);
	void location_removed (ARDOUR::Location *);
	void location_added (ARDOUR::Location *);
	void map_locations (ARDOUR::Locations::LocationList&);

  protected:
	bool on_delete_event (GdkEventAny*);
};

#endif // __ardour_location_ui_h__
