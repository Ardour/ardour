/*
    Copyright (C) 2008 Paul Davis
    Copyright (C) 2015 Waves Audio Ltd.
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

#include "waves_export_file_notebook.h"

#include "ardour/export_format_specification.h"

#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;

WavesExportFileNotebook::WavesExportFileNotebook ()
  : Gtk::VBox ()
  , WavesUI ("waves_export_file_notebook.xml", *this)
  , _format_file_page (0)
  , _format_file_page_home (get_container("file_page_home"))
{
}

WavesExportFileNotebook::~WavesExportFileNotebook ()
{
	if (_format_file_page) {
		delete _format_file_page;
	}
}


void
WavesExportFileNotebook::set_session_and_manager (ARDOUR::Session* s, boost::shared_ptr<ARDOUR::ExportProfileManager> manager)
{
	SessionHandlePtr::set_session (s);
	profile_manager = manager;

	sync_with_manager ();
}

void
WavesExportFileNotebook::sync_with_manager ()
{
	/* File notebook */
	ExportProfileManager::FormatStateList const & formats = profile_manager->get_formats ();
	ExportProfileManager::FormatStateList::const_iterator format_it = formats.begin();

	ExportProfileManager::FilenameStateList const & filenames = profile_manager->get_filenames ();
	ExportProfileManager::FilenameStateList::const_iterator filename_it = filenames.begin ();

	if (_format_file_page) {
        _format_file_page_home.remove (*_format_file_page);
		remove_file_page (_format_file_page);
        _format_file_page = 0;
	}

	// Lossless formats page:
	if ((format_it != formats.end ()) && (filename_it != filenames.end ())) {
		_format_file_page = add_file_page (*format_it, *filename_it);
		_format_file_page_home.add (*_format_file_page);
		_format_file_page_home.show_all ();
	}

	CriticalSelectionChanged ();
}

void
WavesExportFileNotebook::update_soundcloud_upload ()
{
	bool show_credentials_entry = false;
	ExportProfileManager::FormatStateList const & formats = profile_manager->get_formats ();
	ExportProfileManager::FormatStateList::const_iterator format_it = formats.begin();

	if ((format_it != formats.end ()) && _format_file_page) {
		bool this_soundcloud_upload = _format_file_page->get_soundcloud_upload ();
		(*format_it)->format->set_soundcloud_upload (this_soundcloud_upload);
		if (this_soundcloud_upload) {
			show_credentials_entry  = true;
		}
	}

	soundcloud_export_selector->set_visible (show_credentials_entry);
}

void
WavesExportFileNotebook::update_example_filenames ()
{
	if (_format_file_page) {
		_format_file_page->update_example_filename();
	}
}

WavesExportFileNotebook::FilePage*
WavesExportFileNotebook::add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state, ARDOUR::ExportProfileManager::FilenameStatePtr filename_state)
{
	FilePage* page = Gtk::manage (new FilePage (_session, profile_manager, this, format_state, filename_state));
	page->CriticalSelectionChanged.connect (CriticalSelectionChanged.make_slot());

	show_all_children();

	CriticalSelectionChanged ();
	return page;
}

void
WavesExportFileNotebook::remove_file_page (FilePage* page)
{
	profile_manager->remove_format_state (page->get_format_state());
	profile_manager->remove_filename_state (page->get_filename_state());
	delete page;

	CriticalSelectionChanged ();
}

WavesExportFileNotebook::FilePage::FilePage (Session* s,
											 ManagerPtr profile_manager,
											 WavesExportFileNotebook* parent,
											 ExportProfileManager::FormatStatePtr format_state,
											 ExportProfileManager::FilenameStatePtr filename_state)
  : Gtk::VBox()
  , WavesUI ("waves_export_file_notebook_page.xml", *this)
  , format_state (format_state)
  , filename_state (filename_state)
  , profile_manager (profile_manager)
  , _format_selector_home (get_container ("format_selector_home"))
  , _filename_selector_home (get_container ("filename_selector_home"))
{
	/* Set states */
	_format_selector.set_state (format_state, s);
 	_filename_selector.set_state (filename_state, s);

	/* Signals */
	_format_selector.FormatEdited.connect (sigc::mem_fun (*this, &WavesExportFileNotebook::FilePage::save_format_to_manager));

	_format_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &WavesExportFileNotebook::FilePage::critical_selection_changed));
	_filename_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &WavesExportFileNotebook::FilePage::critical_selection_changed));
	
	_format_selector_home.add (_format_selector);
	_filename_selector_home.add (_filename_selector);
	update_example_filename();

	/* Done */

	show_all_children ();
}

WavesExportFileNotebook::FilePage::~FilePage ()
{
}

std::string
WavesExportFileNotebook::FilePage::get_format_name () const
{
	if (format_state && format_state->format) {
		return format_state->format->name();
	}
	return _("No format!");
}

bool
WavesExportFileNotebook::FilePage::get_soundcloud_upload () const
{
	return 0;
//	return soundcloud_upload_button.get_active ();
}

void
WavesExportFileNotebook::FilePage::save_format_to_manager (FormatPtr format)
{
	profile_manager->save_format_to_disk (format);
}

void
WavesExportFileNotebook::FilePage::update_example_filename()
{
	if (profile_manager) {

		std::string example;
		if (format_state->format) {
			example = profile_manager->get_sample_filename_for_format (
				filename_state->filename, format_state->format);
		}
		
		if (example != "") {
			_filename_selector.set_example_filename(Glib::path_get_basename (example));
		} else {
			_filename_selector.set_example_filename("");
		}
	}
}

void
WavesExportFileNotebook::FilePage::critical_selection_changed ()
{
	update_example_filename();
	CriticalSelectionChanged();
}
