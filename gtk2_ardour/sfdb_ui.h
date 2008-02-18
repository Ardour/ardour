/*
    Copyright (C) 2005-2006 Paul Davis

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

#ifndef __ardour_sfdb_ui_h__
#define __ardour_sfdb_ui_h__

#include <string>
#include <vector>
#include <map>
#include <glibmm/ustring.h>

#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserwidget.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>

#include <ardour/session.h>
#include <ardour/audiofilesource.h>

#include "ardour_dialog.h"
#include "editing.h"

namespace ARDOUR {
	class Session;
};

class GainMeter;

class SoundFileBox : public Gtk::VBox
{
  public:
	SoundFileBox (bool persistent);
	virtual ~SoundFileBox () {};
	
	void set_session (ARDOUR::Session* s);
	bool setup_labels (const Glib::ustring& filename);

	void audition();
	bool audition_oneshot();
	bool autoplay () const;

  protected:
	ARDOUR::Session* _session;
	Glib::ustring path;
	
	ARDOUR::SoundFileInfo sf_info;
	
	Gtk::Table table;
	
	Gtk::Label length;
	Gtk::Label format;
	Gtk::Label channels;
	Gtk::Label samplerate;
	Gtk::Label timecode;

	Gtk::Label channels_value;
	Gtk::Label samplerate_value;
	
	Gtk::Label format_text;
	AudioClock length_clock;
	AudioClock timecode_clock;

	Gtk::Frame border_frame;
	Gtk::Label preview_label;

	Gtk::TextView tags_entry;
	
	Gtk::VBox main_box;
	Gtk::VBox path_box;
	Gtk::HBox bottom_box;
	
	Gtk::Button play_btn;
	Gtk::Button stop_btn;
	Gtk::CheckButton autoplay_btn;
	Gtk::Button apply_btn;

	bool tags_entry_left (GdkEventFocus* event);
	void tags_changed ();
	void save_tags (const std::vector<std::string>&);
	void stop_audition ();
};

class SoundFileBrowser : public ArdourDialog
{
  private:
	class FoundTagColumns : public Gtk::TreeModel::ColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<Glib::ustring> pathname;
		
		FoundTagColumns() { add(pathname); }
	};
	
	FoundTagColumns found_list_columns;
	Glib::RefPtr<Gtk::ListStore> found_list;

  public:
	SoundFileBrowser (Gtk::Window& parent, std::string title, ARDOUR::Session* _s, bool persistent);
	virtual ~SoundFileBrowser ();
	
	virtual void set_session (ARDOUR::Session*);
	std::vector<Glib::ustring> get_paths ();
	
	void clear_selection ();

	Gtk::FileChooserWidget chooser;
	Gtk::TreeView found_list_view;

  protected:
	bool resetting_ourselves;
	
	Gtk::FileFilter audio_filter;
	Gtk::FileFilter midi_filter;
	Gtk::FileFilter matchall_filter;
	SoundFileBox preview;
	Gtk::HBox hpacker;

	static Glib::ustring persistent_folder;

	Gtk::Entry found_entry;
	Gtk::Button found_search_btn;
	Gtk::Notebook notebook;

	GainMeter* gm;
	Gtk::VBox meter_packer;
	void add_gain_meter ();
	void remove_gain_meter ();
	void meter ();
	void start_metering ();
	void stop_metering ();
	sigc::connection metering_connection;

	void update_preview ();
	void found_list_view_selected ();
	void found_list_view_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*);
	void found_search_clicked ();

	void chooser_file_activated ();
	
	bool on_audio_filter (const Gtk::FileFilter::Info& filter_info);
	bool on_midi_filter (const Gtk::FileFilter::Info& filter_info);

	virtual bool reset_options() { return true; }

  protected:
	void on_show();

};

class SoundFileChooser : public SoundFileBrowser
{
  public:
	SoundFileChooser (Gtk::Window& parent, std::string title, ARDOUR::Session* _s = 0);
	virtual ~SoundFileChooser () {};
	
	Glib::ustring get_filename ();

  protected:
	void on_hide();
};

class SoundFileOmega : public SoundFileBrowser
{

  public:
	SoundFileOmega (Gtk::Window& parent, std::string title, ARDOUR::Session* _s, int selected_tracks, bool persistent, 
			Editing::ImportMode mode_hint = Editing::ImportAsTrack);
	
	void reset (int selected_tracks);
	
	Gtk::ComboBoxText action_combo;
	Gtk::ComboBoxText where_combo;
	Gtk::ComboBoxText channel_combo;
	Gtk::ComboBoxText src_combo;

	Gtk::CheckButton copy_files_btn;

	void set_mode (Editing::ImportMode);
	Editing::ImportMode get_mode() const;
	Editing::ImportPosition get_position() const;
	Editing::ImportDisposition get_channel_disposition() const;
	ARDOUR::SrcQuality get_src_quality() const;

  protected:
	void on_hide();

  private:
	uint32_t selected_track_cnt;

	typedef std::map<Glib::ustring,Editing::ImportDisposition> DispositionMap;
	DispositionMap disposition_map;

	Gtk::HBox options;
	Gtk::VBox block_two;
	Gtk::VBox block_three;
	Gtk::VBox block_four;

	bool check_info (const std::vector<Glib::ustring>& paths, 
			 bool& same_size, bool& src_needed, bool& multichannel);

	static bool check_link_status (const ARDOUR::Session&, const std::vector<Glib::ustring>& paths);

	void file_selection_changed ();
	bool reset_options ();
	void reset_options_noret ();
	bool bad_file_message ();
};

#endif // __ardour_sfdb_ui_h__
