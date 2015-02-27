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

#ifndef __waves_export_filename_selector_h__
#define __waves_export_filename_selector_h__

#include <gtkmm.h>

#include "ardour/export_profile_manager.h"
#include "ardour/export_filename.h"
#include "ardour/session_handle.h"
#include "waves_ui.h"

///
class WavesExportFilenameSelector : public Gtk::VBox, public WavesUI, public ARDOUR::SessionHandlePtr
{
  public:
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;

	WavesExportFilenameSelector ();
	~WavesExportFilenameSelector ();

	void set_state (ARDOUR::ExportProfileManager::FilenameStatePtr state_, ARDOUR::Session * session_);
	void set_example_filename (std::string filename);

	/* Compatibility with other elements */

	sigc::signal<void> CriticalSelectionChanged;

  private:

	void load_state ();

	void update_label ();
	void update_revision ();

	void change_date_format (WavesDropdown*, int);
	void change_time_format (WavesDropdown*, int);

	void change_session_selection (WavesButton*);
	void change_revision_selection (WavesButton*);
	void on_revision_inc_button (WavesButton*);
	void on_revision_dec_button (WavesButton*);
	void change_revision_value (int);

	void open_browse_dialog (WavesButton*);

	boost::shared_ptr<ARDOUR::ExportFilename> filename;

	WavesDropdown& _date_format_dropdown;
	WavesDropdown& _time_format_dropdown;
	WavesButton& _session_button;
	WavesButton& _revision_button;
	Gtk::Entry& _label_entry;
	Gtk::Label& _path_label;
	Gtk::Entry& _revision_entry;
	WavesButton& _revision_inc_button;
	WavesButton& _revision_dec_button;
	WavesButton& _browse_button;
	Gtk::Label& _example_filename_label;

	/* Date combo */
	typedef ARDOUR::ExportFilename::DateFormat DateFormat;
	typedef ARDOUR::ExportFilename::TimeFormat TimeFormat;
};

#endif // __waves_export_filename_selector_h__
