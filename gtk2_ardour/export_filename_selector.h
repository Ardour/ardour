/*
 * Copyright (C) 2008-2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
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

#ifndef __export_filename_selector_h__
#define __export_filename_selector_h__

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/treemodel.h>

#include "ardour/export_filename.h"
#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"

class ExportFilenameSelector : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;

	ExportFilenameSelector ();
	~ExportFilenameSelector ();

	void set_state (ARDOUR::ExportProfileManager::FilenameStatePtr state_, ARDOUR::Session* session_);
	void set_example_filename (std::string filename);
	void require_timespan (bool);

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
	void change_timespan_selection ();
	void change_revision_selection ();
	void change_revision_value ();

	void open_browse_dialog ();
	void open_folder ();

	boost::shared_ptr<ARDOUR::ExportFilename> filename;

	Glib::RefPtr<Gtk::SizeGroup> label_sizegroup;

	Gtk::Label include_label;

	Gtk::HBox include_hbox;

	Gtk::Label label_label;
	Gtk::Entry label_entry;

	Gtk::ComboBoxText session_snap_name;
	Gtk::CheckButton  timespan_checkbox;

	Gtk::CheckButton revision_checkbox;
	Gtk::SpinButton  revision_spinbutton;

	Gtk::HBox path_hbox;

	Gtk::Label  path_label;
	Gtk::Entry  path_entry;
	Gtk::Button browse_button;
	Gtk::Button open_button;
	Gtk::Label  example_filename_label;

	/* Date combo */

	typedef ARDOUR::ExportFilename::DateFormat DateFormat;

	struct DateFormatCols : public Gtk::TreeModelColumnRecord {
	public:
		Gtk::TreeModelColumn<DateFormat>  format;
		Gtk::TreeModelColumn<std::string> label;

		DateFormatCols ()
		{
			add (format);
			add (label);
		}
	};
	DateFormatCols               date_format_cols;
	Glib::RefPtr<Gtk::ListStore> date_format_list;
	Gtk::ComboBox                date_format_combo;

	/* Time combo */

	typedef ARDOUR::ExportFilename::TimeFormat TimeFormat;

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

	/* timespan logic */
	void update_timespan_sensitivity ();
	bool _require_timespan;
};

#endif /* __export_filename_selector_h__ */
