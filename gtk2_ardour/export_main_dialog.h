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

#ifndef __export_main_dialog_h__
#define __export_main_dialog_h__

#include <ardour/export_handler.h>
#include <ardour/export_profile_manager.h>

#include "public_editor.h"
#include "export_timespan_selector.h"
#include "export_channel_selector.h"
#include "export_format_selector.h"
#include "export_filename_selector.h"
#include "ardour_dialog.h"

#include <gtkmm.h>

namespace ARDOUR {
	class ExportFilename;
	class ExportFormatSpecification;
	class ExportChannelConfiguration;
}

class ExportTimespanSelector;
class ExportChannelSelector;
class ExportFormatSelector;
class ExportFilenameSelector;

class ExportMainDialog : public ArdourDialog {

  public:

	explicit ExportMainDialog (PublicEditor & editor);
	~ExportMainDialog ();
	
	void set_session (ARDOUR::Session* s);
	
	void select_timespan (Glib::ustring id);
	
	/* Responses */

	enum Responses {
		RESPONSE_RT,
		RESPONSE_FAST,
		RESPONSE_CANCEL
	};

  private:
	
	void close_dialog ();
	
	void sync_with_manager ();
	void update_warnings ();
	void show_conflicting_files ();
	
	typedef boost::shared_ptr<ARDOUR::ExportTimespan> TimespanPtr;
	typedef boost::shared_ptr<std::list<TimespanPtr> > TimespanList;
	
	typedef boost::shared_ptr<ARDOUR::ExportChannelConfiguration> ChannelConfigPtr;
	typedef std::list<ChannelConfigPtr> ChannelConfigList;
	
	typedef boost::shared_ptr<ARDOUR::ExportFilename> FilenamePtr;
	
	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;

	void export_rt ();
	void export_fw ();
	
	void show_progress ();
	Glib::ustring get_nth_format_name (uint32_t n);
	gint progress_timeout ();
	
	/* Other stuff */
	
	PublicEditor &  editor;
	HandlerPtr      handler;
	ManagerPtr      profile_manager;
	
	/*** GUI components ***/
	
	Gtk::Table          main_table;
	
	/* Presets */
	
	typedef ARDOUR::ExportProfileManager::PresetPtr PresetPtr;
	typedef ARDOUR::ExportProfileManager::PresetList PresetList;
	
	sigc::connection preset_select_connection;
	
	void select_preset ();
	void save_current_preset ();
	void remove_current_preset ();
	
	struct PresetCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<PresetPtr>      preset;
		Gtk::TreeModelColumn<Glib::ustring>  label;
	
		PresetCols () { add (preset); add (label); }
	};
	PresetCols                   preset_cols;
	Glib::RefPtr<Gtk::ListStore> preset_list;
	PresetPtr                    current_preset;
	PresetPtr                    previous_preset;
	
	Gtk::Alignment      preset_align;
	Gtk::HBox           preset_hbox;
	Gtk::Label          preset_label;
	Gtk::ComboBoxEntry  preset_entry;
	
	Gtk::Button         preset_save_button;
	Gtk::Button         preset_remove_button;
	Gtk::Button         preset_new_button;
	
	/* File Notebook */
	
	class FilePage : public Gtk::VBox {
	  public:
		FilePage (ARDOUR::Session * s, ManagerPtr profile_manager, ExportMainDialog * parent, uint32_t number, 
		          ARDOUR::ExportProfileManager::FormatStatePtr format_state,
		          ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);
		
		virtual ~FilePage ();
		
		Gtk::Widget & get_tab_widget () { return tab_widget; }
		void set_remove_sensitive (bool value);
		Glib::ustring get_format_name () const;
		
		ARDOUR::ExportProfileManager::FormatStatePtr   get_format_state () const { return format_state; }
		ARDOUR::ExportProfileManager::FilenameStatePtr get_filename_state () const { return filename_state; }
		
		sigc::signal<void> CriticalSelectionChanged;
		
	  private:
		void save_format_to_manager (FormatPtr format);
		void update_tab_label ();
		
		ARDOUR::ExportProfileManager::FormatStatePtr   format_state;
		ARDOUR::ExportProfileManager::FilenameStatePtr filename_state;
		ManagerPtr                                     profile_manager;
	
		/* GUI components */
	
		Gtk::Label              format_label;
		Gtk::Alignment          format_align;
		ExportFormatSelector    format_selector;
	
		Gtk::Label              filename_label;
		Gtk::Alignment          filename_align;
		ExportFilenameSelector  filename_selector;
	
		Gtk::HBox               tab_widget;
		Gtk::Label              tab_label;
		Gtk::Alignment          tab_close_alignment;
		Gtk::Button             tab_close_button;
		uint32_t                tab_number;
	};
	
	void add_new_file_page ();
	void add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state, ARDOUR::ExportProfileManager::FilenameStatePtr filename_state);
	void remove_file_page (FilePage * page);
	void update_remove_file_page_sensitivity ();
	
	sigc::connection page_change_connection;
	void handle_page_change (GtkNotebookPage*, uint32_t page);
	
	uint32_t last_visible_page;
	uint32_t page_counter;
	
	/* Warning area */
	
	Gtk::VBox           warn_container;
	
	Gtk::HBox           warn_hbox;
	Gtk::Label          warn_label;
	Glib::ustring       warn_string;
	
	Gtk::HBox           list_files_hbox;
	Gtk::Label          list_files_label;
	Gtk::Button         list_files_button;
	Glib::ustring       list_files_string;
	
	void add_error (Glib::ustring const & text);
	void add_warning (Glib::ustring const & text);
	
	/* Progress bar */
	
	Gtk::VBox               progress_container;
	Gtk::Label              progress_label;
	Gtk::ProgressBar        progress_bar;
	sigc::connection        progress_connection;
	
	/* Everything else */

	Gtk::Label              timespan_label;
	Gtk::Alignment          timespan_align;
	ExportTimespanSelector  timespan_selector;
	
	Gtk::Label              channels_label;
	Gtk::Alignment          channels_align;
	ExportChannelSelector   channel_selector;
	
	Gtk::Notebook           file_notebook;
	
	Gtk::HBox               new_file_hbox;
	Gtk::Button             new_file_button;
	Gtk::VBox               new_file_dummy;
	
	Gtk::Button *           cancel_button;
	Gtk::Button *           rt_export_button;
	Gtk::Button *           fast_export_button;

};

#endif /* __ardour_export_main_dialog_h__ */
