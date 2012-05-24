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

#include "audio_clock.h"

#include <list>

#include <gtkmm.h>
#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/session_handle.h"
#include "ardour/export_profile_manager.h"

namespace ARDOUR {
	class Location;
	class ExportTimespan;
	class ExportHandler;
}

using ARDOUR::CDMarkerFormat;
using ARDOUR::framecnt_t;

/// Timespan Selector base
class ExportTimespanSelector : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
  protected:
	typedef std::list<ARDOUR::Location *> LocationList;
	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ProfileManagerPtr;

	typedef std::list<ARDOUR::ExportTimespanPtr> TimespanList;
	typedef boost::shared_ptr<TimespanList> TimespanListPtr;
	typedef ARDOUR::ExportProfileManager::TimespanStatePtr TimespanStatePtr;

  public:

	ExportTimespanSelector (ARDOUR::Session * session, ProfileManagerPtr manager);

	virtual ~ExportTimespanSelector ();

	void sync_with_manager ();

	sigc::signal<void> CriticalSelectionChanged;

  protected:

	ProfileManagerPtr manager;
	TimespanStatePtr  state;

	virtual void fill_range_list () = 0;

	void add_range_to_selection (ARDOUR::Location const * loc);
	void set_time_format_from_state ();

	void change_time_format ();

	std::string construct_label (ARDOUR::Location const * location) const;
	std::string construct_length (ARDOUR::Location const * location) const;
	std::string bbt_str (framepos_t frames) const;
	std::string timecode_str (framecnt_t frames) const;
	std::string ms_str (framecnt_t frames) const;

	void update_range_name (std::string const & path, std::string const & new_text);

	void set_selection_state_of_all_timespans (bool);

	/*** GUI components ***/

	Gtk::HBox      option_hbox;
	Gtk::Label     time_format_label;

	/* Time format */

	typedef ARDOUR::ExportProfileManager::TimeFormat TimeFormat;

	struct TimeFormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<TimeFormat>      format;
		Gtk::TreeModelColumn<std::string>   label;

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
		Gtk::TreeModelColumn<std::string>       label;
		Gtk::TreeModelColumn<bool>              selected;
		Gtk::TreeModelColumn<std::string>       name;
		Gtk::TreeModelColumn<std::string>       length;

		RangeCols () { add (location); add(label); add(selected); add(name); add(length); }
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
	ExportTimespanSelectorSingle (ARDOUR::Session * session, ProfileManagerPtr manager, std::string range_id);

  private:

	virtual void fill_range_list ();

	std::string range_id;

};

#endif /* __export_timespan_selector_h__ */
