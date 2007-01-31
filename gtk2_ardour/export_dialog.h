/*
    Copyright (C) 1999-2002 Paul Davis 

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

#ifndef __ardour_export_dialog_h__
#define __ardour_export_dialog_h__

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/frame.h>
#include <gtkmm/button.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/fileselection.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include <ardour/export.h>
#include <ardour/location.h>

#include "ardour_dialog.h"

class PublicEditor;

namespace ARDOUR {
	class Session;
	class AudioRegion;
	class Port;
}

class ExportDialog : public ArdourDialog
{
  public:
	ExportDialog (PublicEditor&);
	~ExportDialog ();

	void connect_to_session (ARDOUR::Session*);
	virtual void set_range (nframes_t start, nframes_t end);
	void start_export ();

  protected:
	ARDOUR::AudioExportSpecification spec;

    struct ExportModelColumns : public Gtk::TreeModel::ColumnRecord
	{
	public:
	  Gtk::TreeModelColumn<std::string>	output;
	  Gtk::TreeModelColumn<bool>		left;
	  Gtk::TreeModelColumn<bool>		right;
	  Gtk::TreeModelColumn<ARDOUR::Port*>	port;

	  ExportModelColumns() { add(output); add(left); add(right); add(port);}
	};

	ExportModelColumns exp_cols;
	
	// These methods are intended to be used in constructors of subclasses
	void do_not_allow_track_and_master_selection();
	void do_not_allow_channel_count_selection();
	void do_not_allow_export_cd_markers(); 
	
	// Checks the given filename for validity when export gets started.
	// Export will interrupt when this method returns 'false'.
	// Method is responsible for informing user.
	virtual bool is_filepath_valid(string &filepath);

	// Gets called from within do_export. Is responsible for exporting the
	// audio data. spec has already been filled with user input before calling
	// this method. The dialog will be closed after this function exited.
	virtual void export_audio_data() = 0;

	virtual bool wants_dir() { return false; }
	
	// reads the user input and fills spec with the according values
	// filepath: complete path to the target file, including filename
	void initSpec(string &filepath);

	void set_progress_fraction(double progress) {
			progress_bar.set_fraction (progress); }
	
	ARDOUR::Session& getSession() { return *session; };
	string get_selected_header_format() {
		return header_format_combo.get_active_text(); };
	string get_selected_file_name() { return file_entry.get_text(); };
	
  private:
	PublicEditor&    editor;
	ARDOUR::Session* session;
    bool	track_and_master_selection_allowed;
  	bool	channel_count_selection_allowed;
  	bool	export_cd_markers_allowed;
    
	Gtk::VBox   track_vpacker;
	Gtk::HBox   hpacker;

	Gtk::Table  format_table;
	Gtk::Frame  format_frame;

	Gtk::Label  cue_file_label;
	Gtk::ComboBoxText cue_file_combo;
	
	Gtk::Label  channel_count_label;
	Gtk::ComboBoxText channel_count_combo;

	Gtk::Label  header_format_label;
	Gtk::ComboBoxText header_format_combo;

	Gtk::Label  bitdepth_format_label;
	Gtk::ComboBoxText bitdepth_format_combo;

	Gtk::Label  endian_format_label;
	Gtk::ComboBoxText endian_format_combo;
	
	Gtk::Label  sample_rate_label;
	Gtk::ComboBoxText sample_rate_combo;

	Gtk::Label  src_quality_label;
	Gtk::ComboBoxText src_quality_combo;

	Gtk::Label  dither_type_label;
	Gtk::ComboBoxText dither_type_combo;

	Gtk::CheckButton cuefile_only_checkbox;

	Gtk::Frame  file_frame;
	Gtk::Entry  file_entry;
	Gtk::HBox   file_hbox;
	Gtk::Button file_browse_button;

	Gtk::Button* ok_button;
	Gtk::Button* cancel_button;
	Gtk::Label  cancel_label;
	Gtk::ProgressBar progress_bar;
	Gtk::ScrolledWindow track_scroll;
	Gtk::ScrolledWindow master_scroll;
	Gtk::Button         track_selector_button;
	Gtk::TreeView  track_selector;
	Glib::RefPtr<Gtk::ListStore> track_list;
	Gtk::TreeView  master_selector;
	Glib::RefPtr<Gtk::ListStore> master_list;

	static void *_thread (void *arg);
	// sets the export progress in the progress bar
	virtual gint progress_timeout ();
	sigc::connection progress_connection;
	void build_window ();
	void end_dialog();
	void header_chosen ();
	void channels_chosen ();
	void bitdepth_chosen ();
	void sample_rate_chosen ();
	void cue_file_type_chosen();

	void fill_lists();
	void write_track_and_master_selection_to_spec();

    void do_export_cd_markers (const string& path, const string& cuefile_type);
	void export_cue_file (ARDOUR::Locations::LocationList& locations, const string& path);
	void export_toc_file (ARDOUR::Locations::LocationList& locations, const string& path);
	void do_export ();
	gint window_closed (GdkEventAny *ignored);

	void track_selector_button_click ();

	void browse ();

	void set_state();
	void save_state();
};

#endif // __ardour_export_dialog_h__
