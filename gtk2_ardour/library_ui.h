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

#ifndef __ardour_library_ui_h__
#define __ardour_library_ui_h__

#include <map>
#include <string>
#include <list>
#include <sys/stat.h>
#include <sys/types.h>

#include <sigc++/signal.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/fileselection.h>
#include <gtkmm/notebook.h>
#include <gtkmm2ext/selector.h>

#include <ardour/region.h>

#include "ardour_dialog.h"

using std::string;
using std::map;

struct RowTaggedString {
    RowTaggedString (int r, string s) 
	    : row (r), str (s) {}

    int32_t row;
    string str;
};

class SoundFileBox : public Gtk::VBox 
{
  public:
	/**
	  @variable uri is the path name of string.
	  @variable metadata whether to show the user-added fields from sfdb.
	*/
	SoundFileBox (string uri, bool metadata);
	~SoundFileBox ();

	sigc::signal<void, string, bool> file_chosen;

  private:
	string uri;
	bool metadata;
	SF_INFO* sf_info;

	char* playcmd;
	pid_t current_pid;
	
	Gtk::Label label;
	Gtk::Label path;
	Gtk::Entry path_entry;
	Gtk::Label length;
	Gtk::Label format;
	Gtk::Label channels;
	Gtk::Label samplerate;

	Gtkmm2ext::Selector fields;
	string selected_field;

	Gtk::Frame border_frame;
	
	Gtk::VBox main_box;
	Gtk::HBox top_box;
	Gtk::HBox bottom_box;

	Gtk::Button play_btn;
	Gtk::Button stop_btn;
	Gtk::Button add_field_btn;
	Gtk::Button remove_field_btn;

	static void _fields_refiller (Gtk::TreeView &list, void* arg);
	void fields_refiller (Gtk::TreeView);
	int setup_labels (string uri);
	void setup_fields ();

	void play_btn_clicked ();
	void stop_btn_clicked ();
	void add_field_clicked ();
	void remove_field_clicked ();

	void field_selected (Gtkmm2ext::Selector *selector, 
			     Gtkmm2ext::Selector::Result *re);
	void field_chosen (Gtkmm2ext::Selector *selector, 
			   Gtkmm2ext::Selector::Result *re);
	void audition_status_changed (bool state);
};

class SearchSounds : public ArdourDialog 
{
  public:
	SearchSounds ();
	~SearchSounds ();

	sigc::signal<void, string, bool> file_chosen;

  private:
	Gtk::Button find_btn;

	Gtk::RadioButton and_rbtn;
	Gtk::RadioButton or_rbtn;

	Gtkmm2ext::Selector fields;
	string selected_field;

	Gtk::VBox main_box;
	Gtk::HBox rbtn_box;
	Gtk::HBox bottom_box;

	static void _fields_refiller (Gtk::TreeView&, void* arg);
	void fields_refiller (Gtk::TreeView&);
	void setup_fields ();
	
	void field_selected (Gtkmm2ext::Selector *selector, 
			     Gtkmm2ext::Selector::Result *re);

	void find_btn_clicked ();

	void file_found (string uri, bool multi);
};

class SearchResults : public ArdourDialog
{
  public:
	SearchResults (std::map<std::string,std::string> field_values, bool and_search);
	~SearchResults ();

	sigc::signal<void, std::string, bool> file_chosen;

  private:
	std::map<std::string,std::string> search_info;
	bool search_and;
	std::string selection;

	Gtk::VBox main_box;
	Gtk::HBox hbox;
	Gtk::HBox import_box;
	
	Gtk::Button import_btn;
	Gtk::CheckButton multichan_check;
  
	SoundFileBox* info_box;

	Gtkmm2ext::Selector results;
	static void _results_refiller (Gtk::TreeView &list, void* arg);
	void results_refiller (Gtk::TreeView&);

	void import_clicked ();

	void result_chosen (Gtkmm2ext::Selector *selector, 
			    Gtkmm2ext::Selector::Result *re);
};

class LibraryTree : public Gtk::VBox
{
  public:
	LibraryTree ();
	~LibraryTree ();

	sigc::signal<void, std::string, bool> file_chosen;
	sigc::signal<void> group_selected;
	sigc::signal<void, std::string> member_selected;
	sigc::signal<void> member_deselected;
	sigc::signal<void> deselected;

	std::list<std::string> selection;
	void clear_selection ();
	
  private:
	std::map<std::string, Gtk::TreeViewColumn> uri_mapping;
	std::map<std::string, std::string> uri_parent; // this ugly, but necessary

	std::string current_member;
	std::string current_group;
	
	Gtk::HBox hbox;
	Gtk::VBox framed_box;
	Gtk::HBox btn_box_top;
	Gtk::HBox btn_box_bottom;

	Gtk::ScrolledWindow scroll;
	Gtk::TreeView tree;

	Gtk::Button add_btn;
	Gtk::Button remove_btn;
	Gtk::Button find_btn;
	Gtk::Button folder_btn;

	Gtk::FileSelection files_select;

	void file_ok_clicked ();
	void file_cancel_clicked ();
	
	void add_btn_clicked ();
	void folder_btn_clicked ();
	void remove_btn_clicked ();
	void find_btn_clicked ();

	void file_found (std::string uri, bool multi);

	void cb_group_select (Gtk::TreeViewColumn&, std::string uri);
	void cb_member_select (Gtk::TreeViewColumn&, std::string uri);
	void cb_member_deselect (Gtk::TreeViewColumn&, std::string uri);

	void populate ();
	void subpopulate (Gtk::TreeView&, std::string group);

	void added_group (std::string, std::string);
	void removed_group (std::string);
	void added_member (std::string, std::string);
	void removed_member (std::string);
	
	void cancel_import_clicked ();
};

class SoundFileBrowser : public Gtk::VBox {
  public:
	SoundFileBrowser ();
	~SoundFileBrowser ();
  
	sigc::signal<void> group_selected;
	sigc::signal<void, std::string> member_selected;
	sigc::signal<void> member_deselected;
	sigc::signal<void> deselected;
  
	list<RowTaggedString> selection;
	void clear_selection ();

  private:
	std::string current_member;
	std::string current_group;
	Gtk::FileSelection fs_selector;
	Gtk::TreeView* file_list;

	void dir_list_selected(gint row, gint col, GdkEvent* ev);
	void file_list_selected(gint row, gint col, GdkEvent* ev);
	void file_list_deselected(gint row, gint col, GdkEvent* ev);
  
	std::string safety_check_file(std::string file);
};

class SoundFileSelector : public ArdourDialog {
  public:
	/**
	  @variable action the name given to the action button
	  @variable import is action button sensitive
	  @variable multi does splitting the region by channel make sense here
	  @variable persist should this LibraryTree be hidden or deleted when closed
	  */
	SoundFileSelector ();
	~SoundFileSelector ();
  
	void run (std::string action, bool split_makes_sense, bool hide_after_action = false);
	void get_result (vector<std::string>& paths, bool& split);
	void hide_import_stuff();

	sigc::signal<void,vector<std::string>,bool> Action;
	
  private:
	bool multiable;
	bool hide_after_action;
	bool sfdb;
  
  	void import_btn_clicked ();
	void sfdb_group_selected();
	void browser_group_selected();
	void member_selected(std::string member, bool sfdb);
	void member_deselected(bool sfdb);
	void sfdb_deselected();
	void page_switched(Gtk::Notebook_Helpers::Page* page, guint page_num);
  
	Gtk::HBox main_box;
	Gtk::VBox vbox;
	Gtk::Notebook notebook;
	Gtk::Label sfdb_label;
	Gtk::Label fs_label;
  
	SoundFileBrowser sf_browser;
	LibraryTree sfdb_tree;
  
	Gtk::HBox import_box;
  	Gtk::Button import_btn;
	Gtk::CheckButton split_channels;
	Gtk::CheckButton resample_check;
	
	SoundFileBox* info_box;
};

#endif // __ardour_library_ui_h__
