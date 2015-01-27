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

#ifndef __waves_export_file_notebook_h__
#define __waves_export_file_notebook_h__

#include <sigc++/signal.h>
#include <gtkmm.h>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"

#include "waves_export_format_selector.h"
#include "waves_export_filename_selector.h"
#include "soundcloud_export_selector.h"
#include "waves_ui.h"

class WavesExportFileNotebook : public Gtk::VBox, public WavesUI, public ARDOUR::SessionHandlePtr
{
  public:

	WavesExportFileNotebook ();
	~WavesExportFileNotebook ();

	void set_session_and_manager (ARDOUR::Session * s, boost::shared_ptr<ARDOUR::ExportProfileManager> manager);
	void sync_with_manager ();
	void update_example_filenames();

	boost::shared_ptr<SoundcloudExportSelector> soundcloud_export_selector;

	sigc::signal<void> CriticalSelectionChanged;

  private:

	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;
	class FilePage;

	ManagerPtr        profile_manager;

	FilePage* add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state,
							 ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);
	void remove_file_page (FilePage * page);
	void update_soundcloud_upload ();

	FilePage* _format_file_page;

	Gtk::Container& _format_file_page_home;

	class FilePage : public Gtk::VBox, public WavesUI {
	  public:
		FilePage (ARDOUR::Session * s, ManagerPtr profile_manager, WavesExportFileNotebook * parent,
		          ARDOUR::ExportProfileManager::FormatStatePtr format_state,
		          ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);

		virtual ~FilePage ();

		std::string get_format_name () const;
		bool get_soundcloud_upload () const;

		void update_example_filename();

		ARDOUR::ExportProfileManager::FormatStatePtr   get_format_state () const { return format_state; }
		ARDOUR::ExportProfileManager::FilenameStatePtr get_filename_state () const { return filename_state; }

		sigc::signal<void> CriticalSelectionChanged;

	  private:
		void save_format_to_manager (FormatPtr format);
		void critical_selection_changed ();

		ARDOUR::ExportProfileManager::FormatStatePtr   format_state;
		ARDOUR::ExportProfileManager::FilenameStatePtr filename_state;
		ManagerPtr                                     profile_manager;

		/* GUI components */
		Gtk::Container& _format_selector_home;
		Gtk::Container& _filename_selector_home;
		WavesExportFormatSelector    _format_selector;
		PBD::ScopedConnection   format_connection;

		WavesExportFilenameSelector  _filename_selector;
	};
};

#endif //__waves_export_file_notebook_h__
