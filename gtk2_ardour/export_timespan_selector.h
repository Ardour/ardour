/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __export_timespan_selector_h__
#define __export_timespan_selector_h__

#include "public_editor.h"
#include "audio_clock.h"

#include <list>

#include <gtkmm.h>
#include <boost/shared_ptr.hpp>

#include <ardour/types.h>
#include <ardour/export_profile_manager.h>

namespace ARDOUR {
	class Location;
	class ExportTimespan;
	class ExportHandler;
	class Session;
}

using ARDOUR::CDMarkerFormat;

/// 
class ExportTimespanSelector : public Gtk::VBox {
  private:

	typedef std::list<ARDOUR::Location *> LocationList;
	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;

	typedef boost::shared_ptr<ARDOUR::ExportTimespan> TimespanPtr;
	typedef std::list<TimespanPtr> TimespanList;
	typedef boost::shared_ptr<TimespanList> TimespanListPtr;

  public:

	ExportTimespanSelector ();
	~ExportTimespanSelector ();

	void set_state (ARDOUR::ExportProfileManager::TimespanStatePtr const state_, ARDOUR::Session * session_);
	
	void select_one_range (Glib::ustring id);
	
	/* Compatibility with other elements */
	
	sigc::signal<void> CriticalSelectionChanged;

  private:

	void fill_range_list ();
	void set_selection_from_state ();

	void update_selection ();
	void update_timespans ();
	
	void change_time_format ();
	
	Glib::ustring construct_label (ARDOUR::Location const * location);
	
	Glib::ustring bbt_str (nframes_t frames);
	Glib::ustring smpte_str (nframes_t frames);
	Glib::ustring ms_str (nframes_t frames);

	void update_range_name (Glib::ustring const & path, Glib::ustring const & new_text);

	ARDOUR::Session * session;
	
	ARDOUR::ExportProfileManager::TimespanStatePtr state;

	/*** GUI components ***/
	
	Gtk::HBox      option_hbox;
	Gtk::Label     time_format_label;
	
	/* Time format */
	
	typedef ARDOUR::ExportProfileManager::TimeFormat TimeFormat;
	
	struct TimeFormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<TimeFormat>      format;
		Gtk::TreeModelColumn<Glib::ustring>   label;
	
		TimeFormatCols () { add(format); add(label); }
	};
	TimeFormatCols               time_format_cols;
	Glib::RefPtr<Gtk::ListStore> time_format_list;
	Gtk::ComboBox                time_format_combo;
	
	/* View */
	
	struct RangeCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::Location *>  location;
		Gtk::TreeModelColumn<Glib::ustring>       label;
		Gtk::TreeModelColumn<bool>                selected;
		Gtk::TreeModelColumn<Glib::ustring>       name;
	
		RangeCols () { add (location); add(label); add(selected); add(name); }
	};
	RangeCols                    range_cols;
	
	Glib::RefPtr<Gtk::ListStore> range_list;
	Gtk::TreeView                range_view;
	
	Gtk::ScrolledWindow          range_scroller;

};

#endif /* __export_timespan_selector_h__ */
