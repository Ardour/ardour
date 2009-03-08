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

#include "ardour/types.h"
#include "ardour/export_profile_manager.h"

namespace ARDOUR {
	class Location;
	class ExportTimespan;
	class ExportHandler;
	class Session;
}

using ARDOUR::CDMarkerFormat;

/// Timespan Selector base
class ExportTimespanSelector : public Gtk::VBox {
  protected:
	typedef std::list<ARDOUR::Location *> LocationList;
	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ProfileManagerPtr;

	typedef boost::shared_ptr<ARDOUR::ExportTimespan> TimespanPtr;
	typedef std::list<TimespanPtr> TimespanList;
	typedef boost::shared_ptr<TimespanList> TimespanListPtr;
	typedef ARDOUR::ExportProfileManager::TimespanStatePtr TimespanStatePtr;

  public:

	ExportTimespanSelector (ARDOUR::Session * session, ProfileManagerPtr manager);
	
	virtual ~ExportTimespanSelector ();

	void sync_with_manager ();
	
	sigc::signal<void> CriticalSelectionChanged;

  protected:

	ARDOUR::Session * session;
	ProfileManagerPtr manager;
	TimespanStatePtr  state;

	virtual void fill_range_list () = 0;
	
	void add_range_to_selection (ARDOUR::Location const * loc);
	void set_time_format_from_state ();
	
	void change_time_format ();
	
	Glib::ustring construct_label (ARDOUR::Location const * location) const;
	Glib::ustring bbt_str (nframes_t frames) const;
	Glib::ustring smpte_str (nframes_t frames) const;
	Glib::ustring ms_str (nframes_t frames) const;

	void update_range_name (Glib::ustring const & path, Glib::ustring const & new_text);

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

/// Allows seleting multiple timespans
class ExportTimespanSelectorMultiple : public ExportTimespanSelector
{
  public:
	ExportTimespanSelectorMultiple (ARDOUR::Session * session, ProfileManagerPtr manager);

  private:

	virtual void fill_range_list ();

	void set_selection_from_state ();
	void update_selection ();
	void update_timespans ();
};

/// Displays one timespan
class ExportTimespanSelectorSingle : public ExportTimespanSelector
{
  public:
	ExportTimespanSelectorSingle (ARDOUR::Session * session, ProfileManagerPtr manager, Glib::ustring range_id);

  private:

	virtual void fill_range_list ();
	
	Glib::ustring range_id;

};

#endif /* __export_timespan_selector_h__ */
