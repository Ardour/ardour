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
	
	Gtk::Label length;
	Gtk::Label format;
	Gtk::Label channels;
	Gtk::Label samplerate;
	Gtk::Label timecode;
	
	Gtk::Frame border_frame;
	
	Gtk::Entry tags_entry;
	
	Gtk::VBox main_box;
	Gtk::VBox path_box;
	Gtk::HBox bottom_box;
	
	Gtk::Button play_btn;
	Gtk::Button stop_btn;
	Gtk::Button apply_btn;
	
	bool tags_entry_left (GdkEventFocus* event);
	void stop_btn_clicked ();
	void apply_btn_clicked ();
	
	void audition_status_changed (bool state);
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
	SoundFileBrowser (std::string title, ARDOUR::Session* _s = 0);
	virtual ~SoundFileBrowser ();
	
	virtual void set_session (ARDOUR::Session*);
	std::vector<Glib::ustring> get_paths ();
	
	Gtk::FileChooserWidget chooser;
	Gtk::TreeView found_list_view;

  protected:
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
};

class SoundFileChooser : public ArdourDialog
{
  public:
	SoundFileChooser (std::string title, ARDOUR::Session* _s = 0);
	virtual ~SoundFileChooser () {};
	
	Glib::ustring get_filename ();

  private:
	SoundFileBrowser browser;
};

class SoundFileOptionsDialog : public ArdourDialog
{
  private:
	Gtk::RadioButtonGroup rgroup1;
	Gtk::RadioButtonGroup rgroup2;

  public:
	SoundFileOptionsDialog (Gtk::Window& parent, const vector<Glib::ustring>& p, int selected_tracks);

	Editing::ImportMode mode;

	const std::vector<Glib::ustring>&  paths;

	Gtk::CheckButton split_files;
	Gtk::CheckButton merge_stereo;
	
	Gtk::RadioButton as_tracks;
	Gtk::RadioButton to_tracks;
	Gtk::RadioButton as_regions;
	Gtk::RadioButton as_tape_tracks;
	
	Gtk::RadioButton import;
	Gtk::RadioButton embed;

  private:
	int selected_track_cnt;
	bool selection_includes_multichannel;
	Gtk::VBox block_two;
	Gtk::VBox block_three;
	Gtk::VBox block_four;

	static bool check_multichannel_status (const std::vector<Glib::ustring>& paths);
	void mode_changed ();
};

#endif // __ardour_sfdb_ui_h__
