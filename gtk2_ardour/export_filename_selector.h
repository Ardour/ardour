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

#ifndef __export_filename_selector_h__
#define __export_filename_selector_h__

#include <gtkmm.h>

#include "ardour/export_profile_manager.h"
#include "ardour/export_filename.h"
#include "ardour/session_handle.h"

///
class ExportFilenameSelector : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
  public:
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;

	ExportFilenameSelector ();
	~ExportFilenameSelector ();

	void set_state (ARDOUR::ExportProfileManager::FilenameStatePtr state_, ARDOUR::Session * session_);
	void set_example_filename (std::string filename);

	/* Compatibility with other elements */

	sigc::signal<void> CriticalSelectionChanged;

  private:

	void load_state ();

	void update_label ();
	void update_folder ();
	void check_folder ();

	void change_date_format ();
	void change_time_format ();

	void change_session_selection ();
	void change_revision_selection ();
	void change_revision_value ();

	void open_browse_dialog ();

	boost::shared_ptr<ARDOUR::ExportFilename> filename;

	Glib::RefPtr<Gtk::SizeGroup> label_sizegroup;

	Gtk::Label        include_label;

	Gtk::HBox         include_hbox;

	Gtk::Label        label_label;
	Gtk::Entry        label_entry;

	Gtk::CheckButton  session_checkbox;

	Gtk::CheckButton  revision_checkbox;
	Gtk::SpinButton   revision_spinbutton;

	Gtk::HBox         path_hbox;

	Gtk::Label        path_label;
	Gtk::Entry        path_entry;
	Gtk::Button       browse_button;
	Gtk::Label        example_filename_label;

	/* Date combo */

	typedef ARDOUR::ExportFilename::DateFormat DateFormat;

	struct DateFormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<DateFormat>     format;
		Gtk::TreeModelColumn<std::string>  label;

		DateFormatCols () { add(format); add(label); }
	};
	DateFormatCols               date_format_cols;
	Glib::RefPtr<Gtk::ListStore> date_format_list;
	Gtk::ComboBox                date_format_combo;

	/* Time combo */

	typedef ARDOUR::ExportFilename::TimeFormat TimeFormat;

	struct TimeFormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<TimeFormat>     format;
		Gtk::TreeModelColumn<std::string>  label;

		TimeFormatCols () { add(format); add(label); }
	};
	TimeFormatCols               time_format_cols;
	Glib::RefPtr<Gtk::ListStore> time_format_list;
	Gtk::ComboBox                time_format_combo;

};

#endif /* __export_filename_selector_h__ */
