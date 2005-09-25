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

#include <sigc++/signal_system.h>
#include <gtk--.h>
#include <gtkmmext/selector.h>

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

	SigC::Signal2<void, string, bool> file_chosen;

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

	Gtkmmext::Selector fields;
	string selected_field;

	Gtk::Frame border_frame;
	
	Gtk::VBox main_box;
	Gtk::HBox top_box;
	Gtk::HBox bottom_box;

	Gtk::Button play_btn;
	Gtk::Button stop_btn;
	Gtk::Button add_field_btn;
	Gtk::Button remove_field_btn;

	static void _fields_refiller (Gtk::CList &list, void* arg);
	void fields_refiller (Gtk::CList &clist);
	int setup_labels (string uri);
	void setup_fields ();

	void play_btn_clicked ();
	void stop_btn_clicked ();
	void add_field_clicked ();
	void remove_field_clicked ();

	void field_selected (Gtkmmext::Selector *selector, 
						 Gtkmmext::SelectionResult *re);
	void field_chosen (Gtkmmext::Selector *selector, 
						 Gtkmmext::SelectionResult *re);
	void audition_status_changed (bool state);
};

class SearchSounds : public ArdourDialog 
{
  public:
	SearchSounds ();
	~SearchSounds ();

	SigC::Signal2<void, string, bool> file_chosen;

  private:
	Gtk::Button find_btn;

	Gtk::RadioButton and_rbtn;
	Gtk::RadioButton or_rbtn;

	Gtkmmext::Selector fields;
	string selected_field;

	Gtk::VBox main_box;
	Gtk::HBox rbtn_box;
	Gtk::HBox bottom_box;

	static void _fields_refiller (Gtk::CList &list, void* arg);
	void fields_refiller (Gtk::CList &clist);
	void setup_fields ();

	void field_selected (Gtkmmext::Selector *selector, 
						 Gtkmmext::SelectionResult *re);

	void find_btn_clicked ();

	void file_found (string uri, bool multi);
};

class SearchResults : public ArdourDialog
{
  public:
	SearchResults (map<string,string> field_values, bool and_search);
	~SearchResults ();

	SigC::Signal2<void, string, bool> file_chosen;

  private:
	map<string,string> search_info;
	bool search_and;
	string selection;

	Gtk::VBox main_box;
	Gtk::HBox hbox;
	Gtk::HBox import_box;
	
	Gtk::Button import_btn;
	Gtk::CheckButton multichan_check;
  
	SoundFileBox* info_box;

	Gtkmmext::Selector results;
	static void _results_refiller (Gtk::CList &list, void* arg);
	void results_refiller (Gtk::CList &clist);

	void import_clicked ();

	void result_chosen (Gtkmmext::Selector *selector, 
						 Gtkmmext::SelectionResult *re);
};

class LibraryTree : public Gtk::VBox
{
  public:
	LibraryTree ();
	~LibraryTree ();

	SigC::Signal2<void, string, bool> file_chosen;
	SigC::Signal0<void> group_selected;
	SigC::Signal1<void, string> member_selected;
	SigC::Signal0<void> member_deselected;
	SigC::Signal0<void> deselected;

	list<string> selection;
	void clear_selection ();
	
  private:
	map<string, Gtk::TreeItem*> uri_mapping;
	map<string, string> uri_parent; // this ugly, but necessary

	string current_member;
	string current_group;
	
	Gtk::HBox hbox;
	Gtk::VBox framed_box;
	Gtk::HBox btn_box_top;
	Gtk::HBox btn_box_bottom;

	Gtk::ScrolledWindow scroll;
	Gtk::Tree tree;

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

	void file_found (string uri, bool multi);

	void cb_group_select (Gtk::TreeItem* item, string uri);
	void cb_member_select (Gtk::TreeItem* item, string uri);
	void cb_member_deselect (Gtk::TreeItem* item, string uri);

	void populate ();
	void subpopulate (Gtk::Tree*, string group);

	void added_group (string, string);
	void removed_group (string);
	void added_member (string, string);
	void removed_member (string);
	
	void cancel_import_clicked ();
};

class SoundFileBrowser : public Gtk::VBox {
  public:
	SoundFileBrowser ();
	~SoundFileBrowser ();
  
	SigC::Signal0<void> group_selected;
	SigC::Signal1<void, string> member_selected;
	SigC::Signal0<void> member_deselected;
	SigC::Signal0<void> deselected;
  
	list<RowTaggedString> selection;
	void clear_selection ();

  private:
	string current_member;
	string current_group;
	Gtk::FileSelection fs_selector;
	Gtk::CList* file_list;

	void dir_list_selected(gint row, gint col, GdkEvent* ev);
	void file_list_selected(gint row, gint col, GdkEvent* ev);
	void file_list_deselected(gint row, gint col, GdkEvent* ev);
  
	string safety_check_file(string file);
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
  
	void run (string action, bool split_makes_sense, bool hide_after_action = false);
	void get_result (vector<string>& paths, bool& split);
	void hide_import_stuff();

	SigC::Signal2<void,vector<string>,bool> Action;
	
  private:
	bool multiable;
	bool hide_after_action;
	bool sfdb;
  
  	void import_btn_clicked ();
	void sfdb_group_selected();
	void browser_group_selected();
	void member_selected(string member, bool sfdb);
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
