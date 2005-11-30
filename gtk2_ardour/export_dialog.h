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
	ExportDialog (PublicEditor&, ARDOUR::AudioRegion* r = 0);
	~ExportDialog ();

	void connect_to_session (ARDOUR::Session*);
	void set_range (jack_nframes_t start, jack_nframes_t end);
	void start_export ();

  protected:
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

  private:
	PublicEditor&    editor;
	ARDOUR::Session* session;
	ARDOUR::AudioRegion* audio_region;
	Gtk::VBox   track_vpacker;
	Gtk::HBox   hpacker;
	Gtk::HBox   button_box;

	Gtk::Table  format_table;
	Gtk::Frame  format_frame;

	Gtk::Label  channel_count_label;
	Gtk::Alignment channel_count_align;
	Gtk::ComboBoxText channel_count_combo;

	Gtk::Label  header_format_label;
	Gtk::Alignment header_format_align;
	Gtk::ComboBoxText header_format_combo;

	Gtk::Label  bitdepth_format_label;
	Gtk::Alignment bitdepth_format_align;
	Gtk::ComboBoxText bitdepth_format_combo;

	Gtk::Label  endian_format_label;
	Gtk::Alignment endian_format_align;
	Gtk::ComboBoxText endian_format_combo;
	
	Gtk::Label  sample_rate_label;
	Gtk::Alignment sample_rate_align;
	Gtk::ComboBoxText sample_rate_combo;

	Gtk::Label  src_quality_label;
	Gtk::Alignment src_quality_align;
	Gtk::ComboBoxText src_quality_combo;

	Gtk::Label  dither_type_label;
	Gtk::Alignment dither_type_align;
	Gtk::ComboBoxText dither_type_combo;

	Gtk::Label  cue_file_label;
	Gtk::Alignment cue_file_align;
	Gtk::ComboBoxText cue_file_combo;
	
	Gtk::CheckButton cuefile_only_checkbox;

	Gtk::Frame  file_frame;
	Gtk::Entry  file_entry;
	Gtk::HBox   file_hbox;
	Gtk::Button file_browse_button;

	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Label  cancel_label;
	Gtk::ProgressBar progress_bar;
	Gtk::ScrolledWindow track_scroll;
	Gtk::ScrolledWindow master_scroll;
	Gtk::Button         track_selector_button;
	Gtk::TreeView  track_selector;
	Glib::RefPtr<Gtk::ListStore> track_list;
	Gtk::TreeView  master_selector;
	Glib::RefPtr<Gtk::ListStore> master_list;
	Gtk::FileSelection *file_selector;
	ARDOUR::AudioExportSpecification spec;

	static void *_thread (void *arg);
	gint progress_timeout ();
	sigc::connection progress_connection;
	void build_window ();
	void end_dialog();
	void header_chosen ();
	void channels_chosen ();
	void bitdepth_chosen ();
	void sample_rate_chosen ();
	void cue_file_type_chosen();

	void fill_lists();

      	void do_export_cd_markers (const string& path, const string& cuefile_type);
	void export_cue_file (ARDOUR::Locations::LocationList& locations, const string& path);
	void export_toc_file (ARDOUR::Locations::LocationList& locations, const string& path);
	void do_export ();
	gint window_closed (GdkEventAny *ignored);

	void track_selector_button_click ();

	void initiate_browse ();
	void finish_browse (int status);

	void set_state();
	void save_state();

	static void* _export_region_thread (void *);
	void export_region ();
};

#endif // __ardour_export_dialog_h__

