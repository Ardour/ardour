/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __export_file_notebook_h__
#define __export_file_notebook_h__

#include <sigc++/signal.h>

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/notebook.h>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"

#include "export_format_selector.h"
#include "export_filename_selector.h"
#include "soundcloud_export_selector.h"

class ExportFileNotebook : public Gtk::Notebook, public ARDOUR::SessionHandlePtr
{
public:

	ExportFileNotebook ();

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

	void add_new_file_page ();
	void add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state, ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);
	void remove_file_page (FilePage * page);
	void update_remove_file_page_sensitivity ();
	void update_soundcloud_upload ();

	sigc::connection page_change_connection;
	void handle_page_change (GtkNotebookPage*, uint32_t page);

	Gtk::HBox    new_file_hbox;
	Gtk::Button  new_file_button;
	Gtk::VBox    new_file_dummy;

	uint32_t     last_visible_page;
	uint32_t     page_counter;

	class FilePage : public Gtk::VBox
	{
	public:
		FilePage (ARDOUR::Session * s, ManagerPtr profile_manager, ExportFileNotebook * parent, uint32_t number,
		          ARDOUR::ExportProfileManager::FormatStatePtr format_state,
		          ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);

		virtual ~FilePage ();

		Gtk::Widget & get_tab_widget () { return tab_widget; }
		void set_remove_sensitive (bool value);
		std::string get_format_name () const;
		bool get_soundcloud_upload () const;

		void update_example_filename();

		void update_analysis_button ();
		void update_soundcloud_upload_button ();

		ARDOUR::ExportProfileManager::FormatStatePtr   get_format_state () const { return format_state; }
		ARDOUR::ExportProfileManager::FilenameStatePtr get_filename_state () const { return filename_state; }

		sigc::signal<void> CriticalSelectionChanged;

	private:
		void save_format_to_manager (FormatPtr format);
		void update_tab_label ();
		void critical_selection_changed ();
		void analysis_changed ();
		void soundcloud_upload_changed ();

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

		Gtk::CheckButton        soundcloud_upload_button;
		Gtk::CheckButton        analysis_button;
		Gtk::HBox               tab_widget;
		Gtk::Label              tab_label;
		Gtk::Alignment          tab_close_alignment;
		Gtk::Button             tab_close_button;

		uint32_t                tab_number;

		sigc::connection        soundcloud_button_connection;
		sigc::connection        analysis_button_connection;
	};
};

#endif
