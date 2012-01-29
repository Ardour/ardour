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

#ifndef __export_file_notebook_h__
#define __export_file_notebook_h__

#include <sigc++/signal.h>
#include <gtkmm.h>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"

#include "export_format_selector.h"
#include "export_filename_selector.h"

class ExportFileNotebook : public Gtk::Notebook, public ARDOUR::SessionHandlePtr
{
  public:

	ExportFileNotebook ();

	void set_session_and_manager (ARDOUR::Session * s, boost::shared_ptr<ARDOUR::ExportProfileManager> manager);
	void sync_with_manager ();

	void update_example_filenames();

	std::string get_nth_format_name (uint32_t n);

	sigc::signal<void> CriticalSelectionChanged;

  private:

	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;
	class FilePage;

	ManagerPtr        profile_manager;

	void add_new_file_page ();
	void add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state, ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);
	void remove_file_page (FilePage * page);
	void update_remove_file_page_sensitivity ();

	sigc::connection page_change_connection;
	void handle_page_change (GtkNotebookPage*, uint32_t page);

	Gtk::HBox    new_file_hbox;
	Gtk::Button  new_file_button;
	Gtk::VBox    new_file_dummy;

	uint32_t     last_visible_page;
	uint32_t     page_counter;

	class FilePage : public Gtk::VBox {
	  public:
		FilePage (ARDOUR::Session * s, ManagerPtr profile_manager, ExportFileNotebook * parent, uint32_t number,
		          ARDOUR::ExportProfileManager::FormatStatePtr format_state,
		          ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);

		virtual ~FilePage ();

		Gtk::Widget & get_tab_widget () { return tab_widget; }
		void set_remove_sensitive (bool value);
		std::string get_format_name () const;

		void update_example_filename();

		ARDOUR::ExportProfileManager::FormatStatePtr   get_format_state () const { return format_state; }
		ARDOUR::ExportProfileManager::FilenameStatePtr get_filename_state () const { return filename_state; }

		sigc::signal<void> CriticalSelectionChanged;

	  private:
		void save_format_to_manager (FormatPtr format);
		void update_tab_label ();
		void critical_selection_changed ();

		ARDOUR::ExportProfileManager::FormatStatePtr   format_state;
		ARDOUR::ExportProfileManager::FilenameStatePtr filename_state;
		ManagerPtr                                     profile_manager;

		/* GUI components */

		Gtk::Label              format_label;
		Gtk::Alignment          format_align;
		ExportFormatSelector    format_selector;
		PBD::ScopedConnection   format_connection;

		Gtk::Label              filename_label;
		Gtk::Alignment          filename_align;
		ExportFilenameSelector  filename_selector;

		Gtk::HBox               tab_widget;
		Gtk::Label              tab_label;
		Gtk::Alignment          tab_close_alignment;
		Gtk::Button             tab_close_button;
		uint32_t                tab_number;
	};
};

#endif
