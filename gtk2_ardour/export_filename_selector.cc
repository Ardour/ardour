/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>

#include "pbd/openuri.h"

#include "gtkmm2ext/utils.h"

#include "export_filename_selector.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

ExportFilenameSelector::ExportFilenameSelector ()
	: include_label ("", Gtk::ALIGN_LEFT)
	, label_label (_("Label:"), Gtk::ALIGN_LEFT)
	, timespan_checkbox (_("Timespan Name"))
	, revision_checkbox (_("Revision:"))
	, path_label (_("Folder:"), Gtk::ALIGN_LEFT)
	, browse_button (_("Browse"))
	, open_button (_("Open Folder"))
	, example_filename_label ("", Gtk::ALIGN_LEFT)
	, _require_timespan (false)
{
	include_label.set_markup (_("Build filename(s) from these components:"));

	session_snap_name.append_text (_("No Name"));
	session_snap_name.append_text (_("Session Name"));
	session_snap_name.append_text (_("Snapshot Name"));
	session_snap_name.set_active (0);

	pack_start (path_hbox, false, false, 12);
	pack_start (include_label, false, false, 6);
	pack_start (include_hbox, false, false, 0);
	pack_start (example_filename_label, false, false, 12);

	include_hbox.pack_start (session_snap_name, false, false, 3);
	include_hbox.pack_start (label_label, false, false, 3);
	include_hbox.pack_start (label_entry, false, false, 3);
	include_hbox.pack_start (revision_checkbox, false, false, 3);
	include_hbox.pack_start (revision_spinbutton, false, false, 3);
	include_hbox.pack_start (timespan_checkbox, false, false, 3);
	include_hbox.pack_start (date_format_combo, false, false, 3);
	include_hbox.pack_start (time_format_combo, false, false, 3);

	label_entry.set_activates_default ();

	path_hbox.pack_start (path_label, false, false, 3);
	path_hbox.pack_start (path_entry, true, true, 3);
	path_hbox.pack_start (browse_button, false, false, 3);
	path_hbox.pack_start (open_button, false, false, 3); // maybe Mixbus only ?

	path_entry.set_activates_default ();

	label_sizegroup = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	label_sizegroup->add_widget (label_label);
	label_sizegroup->add_widget (path_label);

	/* Date */

	date_format_list = Gtk::ListStore::create (date_format_cols);
	date_format_combo.set_model (date_format_list);
	date_format_combo.pack_start (date_format_cols.label);

	date_format_combo.signal_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_date_format));

	/* Time */

	time_format_list = Gtk::ListStore::create (time_format_cols);
	time_format_combo.set_model (time_format_list);
	time_format_combo.pack_start (time_format_cols.label);

	time_format_combo.signal_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_time_format));

	/* Revision */

	revision_spinbutton.set_digits (0);
	revision_spinbutton.set_increments (1, 10);
	revision_spinbutton.set_range (1, 1000);
	revision_spinbutton.set_sensitive (false);

	/* Signals */

	label_entry.signal_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::update_label));
	path_entry.signal_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::update_folder));
	path_entry.signal_activate ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::check_folder), false);

	session_snap_name.signal_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_session_selection));
	timespan_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_timespan_selection));

	revision_checkbox.signal_toggled ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_revision_selection));
	revision_spinbutton.signal_value_changed ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::change_revision_value));

	browse_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::open_browse_dialog));
	open_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportFilenameSelector::open_folder));
}

ExportFilenameSelector::~ExportFilenameSelector ()
{
}

void
ExportFilenameSelector::load_state ()
{
	if (!filename) {
		return;
	}

	label_entry.set_text (filename->include_label ? filename->get_label () : "");
	if (filename->include_session) {
		if (filename->use_session_snapshot_name) {
			session_snap_name.set_active (2);
		} else {
			session_snap_name.set_active (1);
		}
	} else {
		session_snap_name.set_active (0);
	}
	timespan_checkbox.set_active (filename->include_timespan);
	revision_checkbox.set_active (filename->include_revision);
	revision_spinbutton.set_value (filename->get_revision ());
	path_entry.set_text (filename->get_folder ());

	Gtk::TreeModel::Children::iterator it;

	for (it = date_format_list->children ().begin (); it != date_format_list->children ().end (); ++it) {
		if (it->get_value (date_format_cols.format) == filename->get_date_format ()) {
			date_format_combo.set_active (it);
		}
	}

	for (it = time_format_list->children ().begin (); it != time_format_list->children ().end (); ++it) {
		if (it->get_value (time_format_cols.format) == filename->get_time_format ()) {
			time_format_combo.set_active (it);
		}
	}
}

void
ExportFilenameSelector::set_state (ARDOUR::ExportProfileManager::FilenameStatePtr state_, ARDOUR::Session* session_)
{
	SessionHandlePtr::set_session (session_);

	filename = state_->filename;

	/* Fill combo boxes */

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row      row;

	/* Dates */

	date_format_list->clear ();

	iter                         = date_format_list->append ();
	row                          = *iter;
	row[date_format_cols.format] = ExportFilename::D_None;
	row[date_format_cols.label]  = filename->get_date_format_str (ExportFilename::D_None);

	iter                         = date_format_list->append ();
	row                          = *iter;
	row[date_format_cols.format] = ExportFilename::D_ISO;
	row[date_format_cols.label]  = filename->get_date_format_str (ExportFilename::D_ISO);

	iter                         = date_format_list->append ();
	row                          = *iter;
	row[date_format_cols.format] = ExportFilename::D_ISOShortY;
	row[date_format_cols.label]  = filename->get_date_format_str (ExportFilename::D_ISOShortY);

	iter                         = date_format_list->append ();
	row                          = *iter;
	row[date_format_cols.format] = ExportFilename::D_BE;
	row[date_format_cols.label]  = filename->get_date_format_str (ExportFilename::D_BE);

	iter                         = date_format_list->append ();
	row                          = *iter;
	row[date_format_cols.format] = ExportFilename::D_BEShortY;
	row[date_format_cols.label]  = filename->get_date_format_str (ExportFilename::D_BEShortY);

	/* Times */

	time_format_list->clear ();

	iter                         = time_format_list->append ();
	row                          = *iter;
	row[time_format_cols.format] = ExportFilename::T_None;
	row[time_format_cols.label]  = filename->get_time_format_str (ExportFilename::T_None);

	iter                         = time_format_list->append ();
	row                          = *iter;
	row[time_format_cols.format] = ExportFilename::T_NoDelim;
	row[time_format_cols.label]  = filename->get_time_format_str (ExportFilename::T_NoDelim);

	iter                         = time_format_list->append ();
	row                          = *iter;
	row[time_format_cols.format] = ExportFilename::T_Delim;
	row[time_format_cols.label]  = filename->get_time_format_str (ExportFilename::T_Delim);

	/* Load state */

	load_state ();
}

void
ExportFilenameSelector::set_example_filename (std::string filename)
{
	if (filename == "") {
		example_filename_label.set_markup (_("<small><i>Sorry, no example filename can be shown at the moment</i></small>"));
	} else {
		example_filename_label.set_markup (string_compose (_("<i>Current (approximate) filename</i>: \"%1\""), filename));
	}
}

void
ExportFilenameSelector::update_label ()
{
	if (!filename) {
		return;
	}

	filename->set_label (label_entry.get_text ());

	filename->include_label = !label_entry.get_text ().empty ();
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::update_folder ()
{
	if (!filename) {
		return;
	}

	filename->set_folder (path_entry.get_text ());
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::check_folder ()
{
	if (!filename) {
		return;
	}

	if (!Glib::file_test (path_entry.get_text (), Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
		Gtk::MessageDialog msg (string_compose (_("%1: this is only the directory/folder name, not the filename.\n"
		                                          "The filename will be chosen from the information just above the folder selector."),
		                                        path_entry.get_text ()));
		msg.run ();
		path_entry.set_text (Glib::path_get_dirname (path_entry.get_text ()));
		filename->set_folder (path_entry.get_text ());
		CriticalSelectionChanged ();
	}
}

void
ExportFilenameSelector::change_date_format ()
{
	if (!filename) {
		return;
	}

	DateFormat format = date_format_combo.get_active ()->get_value (date_format_cols.format);
	filename->set_date_format (format);
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::change_time_format ()
{
	if (!filename) {
		return;
	}

	TimeFormat format = time_format_combo.get_active ()->get_value (time_format_cols.format);
	filename->set_time_format (format);
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::require_timespan (bool r)
{
	_require_timespan = r;
	update_timespan_sensitivity ();
}

void
ExportFilenameSelector::update_timespan_sensitivity ()
{
	bool implicit = _require_timespan;

	if (!implicit && !filename->include_session && !filename->include_label && !filename->include_revision && !filename->include_channel_config && !filename->include_channel && !filename->include_date && !filename->include_format_name) {
		implicit = true;
	}

	// remember prev state, force enable if implicit active.
	if (implicit && !timespan_checkbox.get_inconsistent ()) {
		timespan_checkbox.set_inconsistent (true);
		filename->include_timespan = true;
	} else if (!implicit && timespan_checkbox.get_inconsistent ()) {
		filename->include_timespan = timespan_checkbox.get_active ();
		timespan_checkbox.set_inconsistent (false);
	}
}

void
ExportFilenameSelector::change_timespan_selection ()
{
	if (!filename) {
		return;
	}
	if (timespan_checkbox.get_inconsistent ()) {
		return;
	}

	filename->include_timespan = timespan_checkbox.get_active ();
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::change_session_selection ()
{
	if (!filename) {
		return;
	}

	switch (session_snap_name.get_active_row_number ()) {
		case 1:
			filename->include_session           = true;
			filename->use_session_snapshot_name = false;
			break;
		case 2:
			filename->include_session           = true;
			filename->use_session_snapshot_name = true;
			break;
		default:
			filename->include_session           = false;
			filename->use_session_snapshot_name = false;
			break;
	}
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::change_revision_selection ()
{
	if (!filename) {
		return;
	}

	bool selected              = revision_checkbox.get_active ();
	filename->include_revision = selected;

	revision_spinbutton.set_sensitive (selected);
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::change_revision_value ()
{
	if (!filename) {
		return;
	}

	filename->set_revision ((uint32_t)revision_spinbutton.get_value_as_int ());
	CriticalSelectionChanged ();
}

void
ExportFilenameSelector::open_folder ()
{
	const std::string& dir (path_entry.get_text ());
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
		Gtk::MessageDialog msg (string_compose (_("%1: this is not a valid directory/folder."), dir));
		msg.run ();
		return;
	}
	PBD::open_folder (dir);
}

void
ExportFilenameSelector::open_browse_dialog ()
{
	Gtk::FileChooserDialog dialog (_("Choose export folder"), Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	Gtkmm2ext::add_volume_shortcuts (dialog);
	//dialog.set_transient_for(*this);
	dialog.set_filename (path_entry.get_text ());

	dialog.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

	while (true) {
		int result = dialog.run ();

		if (result == Gtk::RESPONSE_OK) {
			std::string filename = dialog.get_filename ();

			if (!Glib::file_test (filename, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
				Gtk::MessageDialog msg (string_compose (_("%1: this is only the directory/folder name, not the filename.\n"
				                                          "The filename will be chosen from the information just above the folder selector."),
				                                        filename));
				msg.run ();
				continue;
			}

			if (filename.length ()) {
				path_entry.set_text (filename);
				break;
			}
		}
		break;
	}

	CriticalSelectionChanged ();
}
