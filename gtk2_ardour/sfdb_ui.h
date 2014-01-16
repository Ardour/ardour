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

#include <sigc++/signal.h>

#include <gtkmm/stock.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserwidget.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/textview.h>
#include <gtkmm/table.h>
#include <gtkmm/liststore.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/notebook.h>


#include "ardour/audiofilesource.h"
#include "ardour/session_handle.h"

#include "ardour_window.h"
#include "editing.h"
#include "audio_clock.h"

namespace ARDOUR {
	class Session;
};

class GainMeter;
class Mootcher;

class SoundFileBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
  public:
	SoundFileBox (bool persistent);
	virtual ~SoundFileBox () {};

	void set_session (ARDOUR::Session* s);
	bool setup_labels (const std::string& filename);

	void audition();
	bool audition_oneshot();
	bool autoplay () const;

  protected:
	std::string path;

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
	Gtk::HScale seek_slider;

	PBD::ScopedConnectionList auditioner_connections;
	void audition_active(bool);
	void audition_progress(ARDOUR::framecnt_t, ARDOUR::framecnt_t);

	bool tags_entry_left (GdkEventFocus* event);
	void tags_changed ();
	void save_tags (const std::vector<std::string>&);
	void stop_audition ();
	bool seek_button_press(GdkEventButton*);
	bool seek_button_release(GdkEventButton*);
	bool _seeking;
};

class SoundFileBrowser : public ArdourWindow
{
  private:
	class FoundTagColumns : public Gtk::TreeModel::ColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<std::string> pathname;

		FoundTagColumns() { add(pathname); }
	};

	class FreesoundColumns : public Gtk::TreeModel::ColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<std::string> id;
		Gtk::TreeModelColumn<std::string> uri;
		Gtk::TreeModelColumn<std::string> filename;
		Gtk::TreeModelColumn<std::string> duration;
		Gtk::TreeModelColumn<std::string> filesize;
		Gtk::TreeModelColumn<std::string> smplrate;
		Gtk::TreeModelColumn<std::string> license;
		Gtk::TreeModelColumn<bool>        started;

		FreesoundColumns() {
			add(id); 
			add(filename); 
			add(uri);
			add(duration);
			add(filesize);
			add(smplrate);
			add(license);
			add(started);
		}
	};

	FoundTagColumns found_list_columns;
	Glib::RefPtr<Gtk::ListStore> found_list;

	FreesoundColumns freesound_list_columns;
	Glib::RefPtr<Gtk::ListStore> freesound_list;

	Gtk::Button freesound_more_btn;
	Gtk::Button freesound_similar_btn;

	void handle_freesound_results(std::string theString);
  public:
	SoundFileBrowser (std::string title, ARDOUR::Session* _s, bool persistent);
	virtual ~SoundFileBrowser ();

        int run ();
        int status () const { return _status; }
        
	virtual void set_session (ARDOUR::Session*);
	std::vector<std::string> get_paths ();

	void clear_selection ();

	Gtk::FileChooserWidget chooser;

	SoundFileBox preview;

	Gtk::Entry found_entry;
	Gtk::Button found_search_btn;
	Gtk::TreeView found_list_view;

	Gtk::Entry freesound_entry;
	Gtk::ComboBoxText freesound_sort;

	Gtk::Button freesound_search_btn;
	Gtk::TreeView freesound_list_view;
	Gtk::Notebook notebook;

	void freesound_search();
	void refresh_display(std::string ID, std::string file);
	
  protected:
	bool resetting_ourselves;
	int matches;
        int _status;
        bool _done;

	Gtk::FileFilter audio_and_midi_filter;
	Gtk::FileFilter audio_filter;
	Gtk::FileFilter midi_filter;
	Gtk::FileFilter custom_filter;
	Gtk::FileFilter matchall_filter;
	Gtk::HBox hpacker;
        Gtk::VBox vpacker;

        Gtk::Button ok_button;
        Gtk::Button cancel_button;
        Gtk::Button apply_button;

	static std::string persistent_folder;


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

	void freesound_list_view_selected ();
	void freesound_list_view_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*);
	void freesound_search_clicked ();
	void freesound_more_clicked ();
	void freesound_similar_clicked ();
	int freesound_page;
	
	void chooser_file_activated ();
	std::string freesound_get_audio_file(Gtk::TreeIter iter);

	bool on_audio_filter (const Gtk::FileFilter::Info& filter_info);
	bool on_midi_filter (const Gtk::FileFilter::Info& filter_info);
	bool on_audio_and_midi_filter (const Gtk::FileFilter::Info& filter_info);

        void set_action_sensitive (bool);

	virtual bool reset_options() { return true; }

  protected:
	void on_show();
        virtual void do_something (int action);
};

class SoundFileChooser : public SoundFileBrowser
{
  public:
	SoundFileChooser (std::string title, ARDOUR::Session* _s = 0);
	virtual ~SoundFileChooser () {};

	std::string get_filename ();

  protected:
	void on_hide();
};

class SoundFileOmega : public SoundFileBrowser
{

  public:
	SoundFileOmega (std::string title, ARDOUR::Session* _s, 
			uint32_t selected_audio_tracks, uint32_t selected_midi_tracks,
			bool persistent,
			Editing::ImportMode mode_hint = Editing::ImportAsTrack);

	void reset (uint32_t selected_audio_tracks, uint32_t selected_midi_tracks);

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
	uint32_t selected_audio_track_cnt;
	uint32_t selected_midi_track_cnt;

	typedef std::map<std::string,Editing::ImportDisposition> DispositionMap;
	DispositionMap disposition_map;

	Gtk::HBox options;
	Gtk::VBox block_two;
	Gtk::VBox block_three;
	Gtk::VBox block_four;

	bool check_info (const std::vector<std::string>& paths,
			 bool& same_size, bool& src_needed, bool& multichannel);

	static bool check_link_status (const ARDOUR::Session*, const std::vector<std::string>& paths);

	void file_selection_changed ();
	bool reset_options ();
	void reset_options_noret ();
	bool bad_file_message ();

        void do_something (int action);
};

#endif // __ardour_sfdb_ui_h__
