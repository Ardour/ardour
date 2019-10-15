/*
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __export_timespan_selector_h__
#define __export_timespan_selector_h__

#include "audio_clock.h"

#include <ctime>
#include <list>

#ifdef interface
#undef interface
#endif

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Location;
	class ExportTimespan;
	class ExportHandler;
}

using ARDOUR::CDMarkerFormat;
using ARDOUR::samplecnt_t;

/// Timespan Selector base
class ExportTimespanSelector : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
protected:
	typedef std::list<ARDOUR::Location*>                    LocationList;
	typedef boost::shared_ptr<ARDOUR::ExportHandler>        HandlerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ProfileManagerPtr;

	typedef std::list<ARDOUR::ExportTimespanPtr>           TimespanList;
	typedef boost::shared_ptr<TimespanList>                TimespanListPtr;
	typedef ARDOUR::ExportProfileManager::TimespanStatePtr TimespanStatePtr;

public:
	ExportTimespanSelector (ARDOUR::Session* session, ProfileManagerPtr manager, bool multi);

	virtual ~ExportTimespanSelector ();

	void sync_with_manager ();
	virtual void allow_realtime_export (bool);

	sigc::signal<void> CriticalSelectionChanged;

protected:
	ProfileManagerPtr manager;
	TimespanStatePtr  state;
	bool              _realtime_available;

	virtual void fill_range_list ()  = 0;
	virtual void update_timespans () = 0;

	void add_range_to_selection (ARDOUR::Location const* loc, bool rt);
	void set_time_format_from_state ();
	void toggle_realtime ();

	void change_time_format ();

	std::string construct_label (ARDOUR::Location const* location) const;
	std::string construct_length (ARDOUR::Location const* location) const;
	std::string bbt_str (samplepos_t samples) const;
	std::string timecode_str (samplecnt_t samples) const;
	std::string ms_str (samplecnt_t samples) const;

	void update_range_name (std::string const& path, std::string const& new_text);

	void set_selection_state_of_all_timespans (bool);
	int  location_sorter (Gtk::TreeModel::iterator a, Gtk::TreeModel::iterator b);

	/*** GUI components ***/

	Gtk::HBox        option_hbox;
	Gtk::Label       time_format_label;
	Gtk::CheckButton realtime_checkbutton;

	/* Time format */

	typedef ARDOUR::ExportProfileManager::TimeFormat TimeFormat;

	struct TimeFormatCols : public Gtk::TreeModelColumnRecord {
	public:
		Gtk::TreeModelColumn<TimeFormat>  format;
		Gtk::TreeModelColumn<std::string> label;

		TimeFormatCols ()
		{
			add (format);
			add (label);
		}
	};
	TimeFormatCols               time_format_cols;
	Glib::RefPtr<Gtk::ListStore> time_format_list;
	Gtk::ComboBox                time_format_combo;

	/* View */

	struct RangeCols : public Gtk::TreeModelColumnRecord {
	public:
		Gtk::TreeModelColumn<ARDOUR::Location*>   location;
		Gtk::TreeModelColumn<std::string>         label;
		Gtk::TreeModelColumn<bool>                selected;
		Gtk::TreeModelColumn<bool>                realtime;
		Gtk::TreeModelColumn<std::string>         name;
		Gtk::TreeModelColumn<std::string>         length;
		Gtk::TreeModelColumn<std::string>         date;
		Gtk::TreeModelColumn<time_t>              timestamp;
		Gtk::TreeModelColumn<ARDOUR::samplecnt_t> length_actual;
		Gtk::TreeModelColumn<ARDOUR::samplecnt_t> start;

		RangeCols ()
		{
			add (location);
			add (label);
			add (selected);
			add (realtime);
			add (name);
			add (length);
			add (date);
			add (timestamp);
			add (length_actual);
			add (start);
		}
	};
	RangeCols range_cols;

	Glib::RefPtr<Gtk::ListStore> range_list;
	Gtk::TreeView                range_view;

	Gtk::ScrolledWindow range_scroller;
};

/// Allows selecting multiple timespans
class ExportTimespanSelectorMultiple : public ExportTimespanSelector
{
public:
	ExportTimespanSelectorMultiple (ARDOUR::Session* session, ProfileManagerPtr manager);

	void allow_realtime_export (bool);

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
	ExportTimespanSelectorSingle (ARDOUR::Session* session, ProfileManagerPtr manager, std::string range_id);

	void allow_realtime_export (bool);

private:
	virtual void fill_range_list ();
	void         update_timespans ();

	std::string range_id;
};

#endif /* __export_timespan_selector_h__ */
