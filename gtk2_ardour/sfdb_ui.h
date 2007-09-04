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

class SoundFileBox : public Gtk::VBox
{
  public:
	SoundFileBox ();
	virtual ~SoundFileBox () {};
	
	void set_session (ARDOUR::Session* s);
	bool setup_labels (const Glib::ustring& filename);

	void audition();

  protected:
	ARDOUR::Session* _session;
	Glib::ustring path;
	
	ARDOUR::SoundFileInfo sf_info;
	
	pid_t current_pid;

	Gtk::Table table;
	
	Gtk::Label length;
	Gtk::Label format;
	Gtk::Label channels;
	Gtk::Label samplerate;
	Gtk::Label timecode;

	Gtk::Label channels_value;
	Gtk::Label samplerate_value;
	
	Gtk::TextView format_text;
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
	Gtk::Button apply_btn;
	
	bool tags_entry_left (GdkEventFocus* event);
	void stop_btn_clicked ();
	void tags_changed ();
	
	void audition_status_changed (bool state);
	sigc::connection audition_connection;
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
	SoundFileBrowser (Gtk::Window& parent, std::string title, ARDOUR::Session* _s);
	virtual ~SoundFileBrowser ();
	
	virtual void set_session (ARDOUR::Session*);
	std::vector<Glib::ustring> get_paths ();
	
	Gtk::FileChooserWidget chooser;
	Gtk::TreeView found_list_view;

  protected:
	bool resetting_ourselves;
	
	Gtk::FileFilter custom_filter;
	Gtk::FileFilter matchall_filter;
	SoundFileBox preview;

	static Glib::ustring persistent_folder;

	Gtk::Entry found_entry;
	Gtk::Button found_search_btn;
	Gtk::Notebook notebook;

	void update_preview ();
	void found_list_view_selected ();
	void found_list_view_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*);
	void found_search_clicked ();

	void chooser_file_activated ();
	
	bool on_custom (const Gtk::FileFilter::Info& filter_info);

	virtual bool reset_options() { return true; }
};

class SoundFileChooser : public SoundFileBrowser
{
  public:
	SoundFileChooser (Gtk::Window& parent, std::string title, ARDOUR::Session* _s = 0);
	virtual ~SoundFileChooser () {};
	
	Glib::ustring get_filename ();

  private:
	// SoundFileBrowser browser;
};

class SoundFileOmega : public SoundFileBrowser
{
  private:
	Gtk::RadioButtonGroup rgroup1;
	Gtk::RadioButtonGroup rgroup2;

  public:
	SoundFileOmega (Gtk::Window& parent, std::string title, ARDOUR::Session* _s, int selected_tracks);
	
	void reset (int selected_tracks);
	
	Gtk::ComboBoxText action_combo;
	Gtk::ComboBoxText where_combo;
	Gtk::ComboBoxText channel_combo;
	
	Gtk::RadioButton import;
	Gtk::RadioButton embed;

	Editing::ImportMode get_mode() const;
	Editing::ImportPosition get_position() const;
	Editing::ImportDisposition get_channel_disposition() const;

  private:
	uint32_t selected_track_cnt;

	typedef std::map<Glib::ustring,Editing::ImportDisposition> DispositionMap;
	DispositionMap disposition_map;

	Gtk::HBox options;
	Gtk::VBox block_two;
	Gtk::VBox block_three;
	Gtk::VBox block_four;

	static bool check_multichannel_status (const std::vector<Glib::ustring>& paths, bool& same_size, bool& err);
	static bool check_link_status (const ARDOUR::Session&, const std::vector<Glib::ustring>& paths);

	void file_selection_changed ();
	bool reset_options ();
	void reset_options_noret ();
	bool bad_file_message ();
};

#endif // __ardour_sfdb_ui_h__
