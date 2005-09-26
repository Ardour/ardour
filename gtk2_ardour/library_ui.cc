/*
    Copyright (C) 2000-2003 Paul Davis

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

#include <vector>
#include <string>
#include <cstdlib>
#include <cctype>
#include <cerrno>

#include <sys/stat.h>
#include <sndfile.h>
#include <signal.h>
#include <unistd.h>

#include <pbd/basename.h>
#include <pbd/forkexec.h>
#include <pbd/ftw.h>
#include <gtkmm.h>
#include <gtkmm/fileselection.h>
#include <gtkmm2ext/gtk_ui.h>
#include <ardour/audio_library.h>
#include <ardour/audioregion.h>
#include <ardour/region.h>
#include <ardour/session.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/sndfilesource.h>
#include <ardour/utils.h>

#include <gtkmm2ext/doi.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "library_ui.h"
#include "prompter.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtk;
using namespace sigc;

SoundFileSelector::SoundFileSelector ()
	: ArdourDialog ("sound file selector"),
	  vbox(false, 4),
	  sfdb_label(_("Soundfile Library")),
	  fs_label(_("Filesystem")),
	  import_box(false, 4),
	  import_btn (X_("foo")), // force a label 
	  split_channels (_("Split Channels"), 0.0),
	  info_box (0)
{
	set_title(_("ardour: soundfile selector"));
	set_name("SoundFileSelector");
	set_default_size (500, 400);
	set_keyboard_input (true);

	add (main_box);
	main_box.set_border_width (6);

	main_box.pack_start(vbox, true, true);
	vbox.pack_start(notebook, true, true);
	vbox.pack_start(import_box, false, false);

	notebook.set_name("SoundFileSelectorNotebook");
	notebook.append_page(sf_browser, fs_label);
	notebook.append_page(sfdb_tree, sfdb_label);
	
	import_box.set_homogeneous (true);
	import_box.pack_start(import_btn);
	import_box.pack_start (split_channels);

	split_channels.set_active(false);
	split_channels.set_sensitive (false);
	
	delete_event.connect (mem_fun(*this, &ArdourDialog::wm_close_event));
	
	import_btn.signal_clicked().connect (mem_fun(*this, &SoundFileSelector::import_btn_clicked));

	sfdb_tree.group_selected.connect (mem_fun(*this, &SoundFileSelector::sfdb_group_selected));
	sfdb_tree.member_selected.connect (bind (mem_fun(*this, &SoundFileSelector::member_selected), true));
	sf_browser.member_selected.connect (bind (mem_fun(*this, &SoundFileSelector::member_selected), false));
	sf_browser.member_deselected.connect (bind (mem_fun(*this, &SoundFileSelector::member_deselected), false));
	sfdb_tree.deselected.connect (mem_fun(*this, &SoundFileSelector::sfdb_deselected));
	sf_browser.group_selected.connect (mem_fun(*this, &SoundFileSelector::browser_group_selected));
	notebook.switch_page.connect (mem_fun(*this, &SoundFileSelector::page_switched));
}

SoundFileSelector::~SoundFileSelector()
{
}

void
SoundFileSelector::import_btn_clicked ()
{
	vector<string> paths;

	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();
	if (sess) {
		sess->cancel_audition();
	}

	if (sfdb) {
		for (list<string>::iterator i = sfdb_tree.selection.begin(); i != sfdb_tree.selection.end(); ++i) {
			paths.push_back (Library->get_member_filename (*i));
		}
	} else {
		for (list<RowTaggedString>::iterator i = sf_browser.selection.begin(); i != sf_browser.selection.end(); ++i) {
			paths.push_back ((*i).str);
		}
	}

	Action (paths, split_channels.get_active()); /* EMIT_SIGNAL */
	
	if (sfdb) {
		sfdb_tree.clear_selection ();
	} else {
		sf_browser.clear_selection ();
	}

	if (hide_after_action) {
	        hide ();
	        Action.clear();
	}
	hide_after_action = false;

}

void
SoundFileSelector::run (string action, bool multi, bool hide_after)
{
	static_cast<Label*>(import_btn.get_child())->set_text (action);
	import_btn.set_sensitive(false);

	if (multi) {
		split_channels.show ();
	} else {
		split_channels.hide ();
	}

	multiable = multi;
	hide_after_action = hide_after;

	set_position (Gtk::WIN_POS_MOUSE);
	ArdourDialog::run ();
}

void
SoundFileSelector::hide_import_stuff()
{
	import_box.hide_all();
}

void
SoundFileSelector::page_switched(Gtk::Notebook_Helpers::Page* page, guint page_num)
{
	if (page_num == 1) {
		sfdb = true;
		if (!sfdb_tree.selection.empty()) {
			member_selected (sfdb_tree.selection.back(), true);
		}
	} else {
		sfdb = false;
		if (!sf_browser.selection.empty()) {
			member_selected (sf_browser.selection.back().str, false);
		}
	}
}

void
SoundFileSelector::sfdb_deselected()
{
	import_btn.set_sensitive(false);
}

void
SoundFileSelector::browser_group_selected()
{
	sfdb_group_selected();
}

void
SoundFileSelector::sfdb_group_selected()
{
	import_btn.set_sensitive(false);
	split_channels.set_sensitive(false);
		
	if (info_box) {
		delete info_box;
		info_box = 0;
	}
}

void
SoundFileSelector::member_selected(string member, bool sfdb)
{
	if (info_box) {
		delete info_box;
		info_box = 0;
	}

	if (member.empty()) {
		return;
	}

	try { 
		info_box = new SoundFileBox(member, sfdb);
	} catch (failed_constructor& err) {
		/* nothing to do */
		return;
	}

	main_box.pack_start (*info_box, false, false);

	import_btn.set_sensitive (true);
	
	if (multiable) {
		split_channels.set_sensitive(true);
	}
}

void
SoundFileSelector::member_deselected (bool sfdb)
{
	bool keep_action_available;
	string last;

	if (info_box) {
		delete info_box;
		info_box = 0;
	}

	if (sfdb) {
		if ((keep_action_available = !sfdb_tree.selection.empty())) {
			last = sfdb_tree.selection.back();
		}
		
	} else {
		if ((keep_action_available = !sf_browser.selection.empty())) {
			last = sf_browser.selection.back().str;
		}
	}
	
	if (keep_action_available) {

		if (info_box) {
			delete info_box;
			info_box = 0;
		}

		try {
			info_box = new SoundFileBox(last, sfdb);
		} catch (failed_constructor& err) {
			/* nothing to do */
			return;
		}

		import_btn.set_sensitive (true);

		if (multiable) {
			split_channels.set_sensitive(true);
		}

		main_box.pack_start(*info_box, false, false);
	}
}

void
SoundFileSelector::get_result (vector<string>& paths, bool& split)
{
	if (sfdb) {
		for (list<string>::iterator i = sfdb_tree.selection.begin(); i != sfdb_tree.selection.end(); ++i) {
			paths.push_back (Library->get_member_filename (*i));
		}
	} else {
		for (list<RowTaggedString>::iterator i = sf_browser.selection.begin(); i != sf_browser.selection.end(); ++i) {
			paths.push_back ((*i).str);
		}
	}

	split = split_channels.get_active();
}

SoundFileBrowser::SoundFileBrowser()
	:
	Gtk::VBox(false, 3)
{
	fs_selector.hide_fileop_buttons();
	fs_selector.set_filename("/");
	
	// This is ugly ugly ugly.  But gtk1 (and gtk2) don't give us any
	// choice.
	GtkFileSelection* fs_gtk = fs_selector.gobj();
	file_list= Gtk::wrap((GtkCList*)(fs_gtk->file_list));

	Gtk::VBox* vbox = manage(new Gtk::VBox);
	Gtk::HBox* tmphbox = manage(new Gtk::HBox);
	Gtk::OptionMenu* option_menu = Gtk::wrap((GtkOptionMenu*)(fs_gtk->history_pulldown));
	option_menu->reparent(*tmphbox);
	vbox->pack_start(*tmphbox, false, false);
	
	/* XXX This interface isn't supported in gtkmm.  Redo it with a BoxList&
	vbox->set_child_packing(*option_menu, false, false); */
	
	Gtk::HBox* hbox = manage(new Gtk::HBox);
	Gtk::ScrolledWindow* dir_scroll = manage(new Gtk::ScrolledWindow);
	Gtk::ScrolledWindow* file_scroll = manage(new Gtk::ScrolledWindow);
	dir_scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	file_scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	Gtk::CList* dir_list = Gtk::wrap((GtkCList*)(fs_gtk->dir_list));

	dir_list->reparent(*dir_scroll);
	file_list->reparent(*file_scroll);
	file_list->set_selection_mode (GTK_SELECTION_MULTIPLE);
	hbox->pack_start(*dir_scroll);
	hbox->pack_start(*file_scroll);
	vbox->pack_start(*hbox, true, true);

	Gtk::VBox* tmpvbox = manage(new Gtk::VBox);
	
	Gtk::Label* selection_text = Gtk::wrap((GtkLabel*)(fs_gtk->selection_text));
	selection_text->reparent(*tmpvbox);
	Gtk::Entry* selection_entry= Gtk::wrap((GtkEntry*)(fs_gtk->selection_entry));
	selection_entry->reparent(*tmpvbox);
	vbox->pack_start(*tmpvbox, false, false);
	
	pack_start(*vbox, true, true);
	
	dir_list->select_row.connect(mem_fun(*this, &SoundFileBrowser::dir_list_selected));
	file_list->select_row.connect(mem_fun(*this, &SoundFileBrowser::file_list_selected));
	file_list->unselect_row.connect(mem_fun(*this, &SoundFileBrowser::file_list_deselected));

	dir_list->set_name("SoundFileBrowserList");
	file_list->set_name("SoundFileBrowserList");
}

SoundFileBrowser::~SoundFileBrowser()
{
}

void
SoundFileBrowser::clear_selection ()
{
	file_list->selection().clear ();
	selection.clear ();
}

void
SoundFileBrowser::dir_list_selected(gint row, gint col, GdkEvent* ev)
{
	current_member = "";
	current_group = "";
	
	group_selected(); /* EMIT_SIGNAL */
}

void
SoundFileBrowser::file_list_selected(gint row, gint col, GdkEvent* ev)
{
	current_group = "";
	current_member = fs_selector.get_filename();

	selection.push_back (RowTaggedString (row, current_member));
	
	member_selected(safety_check_file(current_member)); /* EMIT_SIGNAL */
}

void
SoundFileBrowser::file_list_deselected(gint row, gint col, GdkEvent* ev)
{
	current_group = "";
	current_member = file_list->cell (row, 0).get_text();

	for (list<RowTaggedString>::iterator i = selection.begin(); i != selection.end(); ++i) {
		if ((*i).row == row) {
			selection.erase (i);
			break;
		}
	}

	member_deselected(); /* EMIT_SIGNAL */
}

string
SoundFileBrowser::safety_check_file(string file)
{
	if (file.rfind(".wav") == string::npos &&
		file.rfind(".aiff")== string::npos &&
		file.rfind(".aif") == string::npos &&
		file.rfind(".snd") == string::npos &&
		file.rfind(".au")  == string::npos &&
		file.rfind(".raw") == string::npos &&
		file.rfind(".sf")  == string::npos &&
		file.rfind(".cdr") == string::npos &&
		file.rfind(".smp") == string::npos &&
		file.rfind(".maud")== string::npos &&
		file.rfind(".vwe") == string::npos &&
		file.rfind(".paf") == string::npos &&
		file.rfind(".voc") == string::npos) {
		return "";
	} else {
		return file;
	}
}

static int32_t process_node (const char *file, const struct stat *sb, int32_t flag);
static string length2string (const int, const int);

LibraryTree::LibraryTree ()
	: Gtk::VBox(false, 3),
	  btn_box_top(true, 4),
	  btn_box_bottom (true, 4),
	  add_btn(_("Add to Library...")),
	  remove_btn(_("Remove...")),
	  find_btn(_("Find...")),
	  folder_btn(_("Add Folder")),
	  files_select(_("Add audio file or directory"))
{
	set_border_width (3);

	pack_start(hbox, true, true);
	pack_start(btn_box_top, false, false);
	pack_start(btn_box_bottom, false, false);

	hbox.pack_start(scroll);
	scroll.set_size_request (200, 150);
	scroll.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroll.add_with_viewport(tree);
	tree.set_selection_mode(GTK_SELECTION_MULTIPLE);

	btn_box_top.pack_start(add_btn);
	btn_box_top.pack_start(folder_btn);
	btn_box_top.pack_start(remove_btn);

	btn_box_bottom.pack_start(find_btn);

	remove_btn.set_sensitive (false);

	add_btn.signal_clicked().connect (mem_fun(*this, &LibraryTree::add_btn_clicked));
	folder_btn.signal_clicked().connect (mem_fun(*this, &LibraryTree::folder_btn_clicked));
	remove_btn.signal_clicked().connect (mem_fun(*this, &LibraryTree::remove_btn_clicked));
	find_btn.signal_clicked().connect (mem_fun(*this, &LibraryTree::find_btn_clicked));

	files_select.hide_fileop_buttons();
	files_select.set_filename("/");
	files_select.get_ok_button()-.signal_clicked().connect (mem_fun ( *this, 
				&LibraryTree::file_ok_clicked));
	files_select.get_cancel_button()-.signal_clicked().connect (mem_fun ( *this, 
				&LibraryTree::file_cancel_clicked));


	Library->added_group.connect (mem_fun(*this, &LibraryTree::added_group));
	Library->removed_group.connect (mem_fun(*this, &LibraryTree::removed_group));
	Library->added_member.connect (mem_fun(*this, &LibraryTree::added_member));
	Library->removed_member.connect (mem_fun(*this, &LibraryTree::removed_member));
	
	current_group = "";
	current_member = "";

	populate ();
}

LibraryTree::~LibraryTree ()
{
}

void
LibraryTree::clear_selection ()
{
	using namespace Gtk::Tree_Helpers;
	for (SelectionList::iterator i = tree.selection().begin(); i != tree.selection().end(); ++i) {
		(*i)->deselect ();
	}
	selection.clear ();
}

void
LibraryTree::added_group (string group, string parent)
{
	using namespace Gtk;
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LibraryTree::added_group), group, parent));

	Tree* parent_tree;
	if (parent.length()) {
		parent_tree = uri_mapping[parent]->get_subtree();
	} else {
		parent_tree = &tree;
	}
	
	TreeItem *item = manage(new TreeItem(Library->get_label(group)));
	Tree_Helpers::ItemList items = parent_tree->tree();
	Tree_Helpers::ItemList::iterator i = items.begin();
	
	list<string> groups;
	Library->get_groups(groups, parent);
	list<string>::iterator j = groups.begin();

	while (i != items.end() && j != groups.end()){
		if ((cmp_nocase(Library->get_label(group),Library->get_label(*j)) <= 0) || 
			!((*i)->get_subtree())){
			
			break;
		}
		++i;
		++j;
	}

	parent_tree->tree().insert (i, *item);
	Tree *subtree = manage(new Tree());
	item->set_subtree (*subtree);
	item->expand();

	item->select.connect (bind(mem_fun(*this,&LibraryTree::cb_group_select), item, group));
	
	uri_mapping.insert(map<string, TreeItem*>::value_type(group, item));
	uri_parent.insert(map<string,string>::value_type(group, parent));

	subtree->show();
	item->show();
	
	while (gtk_events_pending()){
		gtk_main_iteration();
	}
}

void
LibraryTree::removed_group (string group)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LibraryTree::removed_group), group));
	
	Gtk::TreeItem* group_item = uri_mapping[group];

	Gtk::Tree* parent_tree;
	if (uri_parent[group].length()) {
		parent_tree = uri_mapping[uri_parent[group]]->get_subtree();
	} else {
		parent_tree = &tree;
	}
	
	parent_tree->tree().remove(*group_item);
	uri_mapping.erase(uri_mapping.find(group));
	uri_parent.erase(uri_parent.find(group));
	
	while (gtk_events_pending()){
		gtk_main_iteration();
	}
}

void
LibraryTree::added_member (string member, string parent)
{
	using namespace Gtk;

	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LibraryTree::added_member), member, parent));

	Tree* parent_tree;
	if (parent.length()) {
		parent_tree = uri_mapping[parent]->get_subtree();
	} else {
		parent_tree = &tree;
	}
	
	TreeItem *item = manage(new TreeItem(Library->get_label(member)));
	Tree_Helpers::ItemList items = parent_tree->tree();
	Tree_Helpers::ItemList::iterator i = items.begin();
	
	list<string> members;
	Library->get_members(members, parent);
	list<string>::iterator j = members.begin();

	while (i != items.end() && j != members.end()){
		if (cmp_nocase(Library->get_label(member), Library->get_label(*j)) <= 0){
			break;
		}
		++i;
		++j;
	}
	
	parent_tree->tree().insert (i, *item);

	item->select.connect 
			(bind(mem_fun(*this,&LibraryTree::cb_member_select), item, member));
	item->deselect.connect 
			(bind(mem_fun(*this,&LibraryTree::cb_member_deselect), item, member));
	
	uri_mapping.insert(map<string, TreeItem*>::value_type(member, item));
	uri_parent.insert(map<string,string>::value_type(member, parent));

	item->show();
	
	while (gtk_events_pending()){
		gtk_main_iteration();
	}
}

void
LibraryTree::removed_member (string member)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &LibraryTree::removed_member), member));
	
	Gtk::TreeItem* member_item = uri_mapping[member];

	Gtk::Tree* parent_tree;
	if (uri_parent[member].length()) {
		parent_tree = uri_mapping[uri_parent[member]]->get_subtree();
	} else {
		parent_tree = &tree;
	}
	
	parent_tree->tree().remove(*member_item);
	uri_mapping.erase(uri_mapping.find(member));
	uri_parent.erase(uri_parent.find(member));
	
	while (gtk_events_pending()){
		gtk_main_iteration();
	}
}

void
LibraryTree::populate ()
{
	subpopulate (&tree, current_group);
}

void
LibraryTree::subpopulate (Gtk::Tree* tree, string group)
{
	using namespace Gtk;

	list<string> groups;
	Library->get_groups(groups, group);
	
	list<string>::iterator i; 

	for (i = groups.begin(); i != groups.end(); ++i) {
		TreeItem *item = 
			manage(new TreeItem(Library->get_label(*i)));
		tree->append (*item);
		Tree *subtree = manage(new Tree());
		item->set_subtree (*subtree);
	
		uri_mapping.insert(map<string, Gtk::TreeItem*>::value_type(*i, item));
		uri_parent.insert(map<string,string>::value_type(*i, group));

		item->select.connect 
				(bind(mem_fun(*this,&LibraryTree::cb_group_select), item, *i));

		subpopulate (subtree, *i);
		subtree->show();
		item->expand();
		item->show();
	}

	list<string> members;
	Library->get_members(members, group);
	for (i = members.begin(); i != members.end(); ++i) {
		TreeItem *item = manage(new TreeItem(Library->get_label(*i)));
		tree->append (*item);
		item->show();

		uri_mapping.insert(map<string, Gtk::TreeItem*>::value_type(*i, item));
		uri_parent.insert(map<string,string>::value_type(*i, group));

		item->select.connect 
				(bind(mem_fun(*this,&LibraryTree::cb_member_select), item, *i));
		item->deselect.connect 
			(bind(mem_fun(*this,&LibraryTree::cb_member_deselect), item, *i));

	}
}

void
LibraryTree::add_btn_clicked ()
{
	files_select.show_all();
}

// Gah, too many globals
static string parent_uri;
static vector<string>* old_parent;
static vector<string>* old_parent_uri;

static void clone_ftw(void*);
static int32_t ftw_return;
static Gtk::ProgressBar* bar;

void
LibraryTree::file_ok_clicked ()
{
	files_select.hide_all();

	string* file = new string(files_select.get_filename());
	parent_uri = current_group;

	Gtk::Window* progress_win = new Gtk::Window();
	progress_win->set_title(_("Importing"));
	progress_win->set_policy(false, false, true);
	Gtk::VBox* main_box = manage(new Gtk::VBox());
	progress_win->add(*main_box);
	bar = manage(new Gtk::ProgressBar());
	bar->set_activity_mode(true);
	bar->set_activity_step(15);
	bar->set_activity_blocks(10);
	main_box->pack_start(*bar);
	Gtk::Button* cancel_btn = manage(new Gtk::Button(_("Cancel")));
	main_box->pack_start(*cancel_btn);
	cancel_btn-.signal_clicked().connect (mem_fun(*this, &LibraryTree::cancel_import_clicked));
	progress_win->show_all();
	
	clone_ftw((void*)file);
	
	delete progress_win;
}

void
LibraryTree::cancel_import_clicked()
{
	ftw_return = 1;
}

void
clone_ftw(void* ptr)
{
	string* file = (string*) ptr;
	
	old_parent = new vector<string>;
	old_parent_uri = new vector<string>;
	ftw_return = 0;

	if (ftw (file->c_str(), process_node, 100) < 0){
		warning << compose(_("%1 not added to database"), *file) << endmsg;
	}

	delete old_parent;
	delete old_parent_uri;

	delete file;
}

void
LibraryTree::file_cancel_clicked ()
{
	files_select.hide_all();
}

void
LibraryTree::folder_btn_clicked ()
{
	ArdourPrompter prompter (true);
	prompter.set_prompt(_("Folder name:"));

	prompter.done.connect(Gtk::Main::quit.slot());
	prompter.show_all();

	Gtk::Main::run();

	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string name;

		prompter.get_result(name);

		if (name.length()) {
			Library->add_group(name, current_group);
		}
	}
}

void
LibraryTree::cb_group_select (Gtk::TreeItem* item, string uri)
{
	current_group = uri;
	current_member = "";
	remove_btn.set_sensitive(true);

	group_selected(); /* EMIT_SIGNAL */
}

void
LibraryTree::cb_member_select (Gtk::TreeItem* item, string uri)
{
	current_member = uri;
	current_group = "";
	selection.push_back (uri);
	member_selected(uri); /* EMIT_SIGNAL */
	remove_btn.set_sensitive(true);
}

void
LibraryTree::cb_member_deselect (Gtk::TreeItem* item, string uri)
{
	current_member = "";
	current_group = "";
	selection.remove (uri);

	member_deselected(); /* EMIT_SIGNAL */
}

void
LibraryTree::find_btn_clicked ()
{
	SearchSounds* search = new SearchSounds ();

	search->file_chosen.connect(mem_fun(*this, &LibraryTree::file_found));
	search->show_all();
}

void
LibraryTree::file_found (string uri, bool multi)
{
	file_chosen (Library->get_member_filename(uri), multi); /* EMIT_SIGNAL */
}

void
LibraryTree::remove_btn_clicked ()
{
	if (current_member != ""){
		Library->remove_member(current_member);
	} else if (current_group != ""){
		Library->remove_group(current_group);
	} else {
		error << _("Should not be reached") << endmsg;
	}

	current_member = "";
	current_group = "";
	
	remove_btn.set_sensitive(false);
	
	deselected(); /* EMIT_SIGNAL */
}

string
length2string (const int32_t frames, const int32_t sample_rate)
{
	int secs = (int) (frames / (float) sample_rate);
	int hrs =  secs / 3600;
	secs -= (hrs * 3600);
	int mins = secs / 60;
	secs -= (mins * 60);

	int total_secs = (hrs * 3600) + (mins * 60) + secs;
	int frames_remaining = frames - (total_secs * sample_rate);
	float fractional_secs = (float) frames_remaining / sample_rate;

	char duration_str[32];
	sprintf (duration_str, "%02d:%02d:%05.2f", hrs, mins, (float) secs + fractional_secs);

	return duration_str;
}

int
process_node (const char *file, const struct stat *sb, int32_t flag)
{
	bar->set_value(0.0);
	while (gtk_events_pending()){
		gtk_main_iteration();
	}
	bar->set_value(1.0);

	string s_file(file);

	if (s_file.find("/.") != string::npos){
		return ftw_return;
	}

	if (flag == FTW_D) {
		string::size_type size = s_file.find_last_of('/');
		string label = s_file.substr(++size);
		
		while (!old_parent->empty() 
			&& (s_file.find(old_parent->back()) == string::npos)) {

			parent_uri = old_parent_uri->back();			
			old_parent_uri->pop_back();
			old_parent->pop_back();
		}

		string uri = Library->add_group(label, parent_uri);

		old_parent->push_back(s_file);
		old_parent_uri->push_back(parent_uri);
		parent_uri = uri;

		return ftw_return;
	} else if (flag != FTW_F) {
		return ftw_return;
	}

	// We can't realistically check every file - or can we ?
	char* suffix;
	if ((suffix = strrchr (file, '.')) == 0) {
		return ftw_return;
	}

	if (*(suffix+1) == '\0') {
		return ftw_return;
	}

    if (strcasecmp (suffix+1, "wav") != 0 &&
        strcasecmp (suffix+1, "aiff") != 0 &&
        strcasecmp (suffix+1, "aif") != 0 &&
        strcasecmp (suffix+1, "snd") != 0 &&
        strcasecmp (suffix+1, "au") != 0 &&
        strcasecmp (suffix+1, "raw") != 0 &&
        strcasecmp (suffix+1, "sf") != 0 &&
        strcasecmp (suffix+1, "cdr") != 0 &&
        strcasecmp (suffix+1, "smp") != 0 &&
        strcasecmp (suffix+1, "maud") != 0 &&
        strcasecmp (suffix+1, "vwe") != 0 &&
		strcasecmp (suffix+1, "paf") != 0 &&
		strcasecmp (suffix+1, "voc") != 0) {

        return ftw_return;
    }

	/* OK, it stands a good chance of being a sound file that we
	   might be able to handle.
	*/

	SNDFILE *sf;
	SF_INFO info;
	if ((sf = sf_open ((char *) file, SFM_READ, &info)) < 0) {
		error << compose(_("file \"%1\" could not be opened"), file) << endmsg;
		return ftw_return;
	}
	sf_close (sf);

	string uri = Library->add_member(file, parent_uri);

	Library->set_field(uri, "channels", compose("%1", info.channels));
	Library->set_field(uri, "samplerate", compose("%1", info.samplerate));
	Library->set_field(uri, "resolution", compose("%1", sndfile_data_width(info.format)));
	Library->set_field(uri, "format", compose("%1", info.format));
	
	return ftw_return;
}

static const gchar* selector_titles[] = {
	N_("Field"), 
	N_("Value"), 
	0
};

SoundFileBox::SoundFileBox (string uri, bool meta)
	:
	uri(uri),
	metadata(meta),
	sf_info(new SF_INFO),
	current_pid(0),
	fields(_fields_refiller, this, internationalize (selector_titles), 
		   false, true),
	main_box (false, 3),
	top_box (true, 4),
	bottom_box (true, 4),
	play_btn(_("Play")),
	stop_btn(_("Stop")),
	add_field_btn(_("Add Field...")),
	remove_field_btn(_("Remove Field"))
{
	set_name ("SoundFileBox");

	border_frame.set_label (_("Soundfile Info"));
	border_frame.add (main_box);

	pack_start (border_frame);
	set_border_width (4);

	Gtk::HBox* path_box = manage (new HBox);
	
	path_box->set_spacing (4);
	path_box->pack_start (path, false, false);
	path_box->pack_start (path_entry, true, true);

	main_box.set_border_width (4);

	main_box.pack_start(label, false, false);
	main_box.pack_start(*path_box, false, false);
	main_box.pack_start(length, false, false);
	main_box.pack_start(format, false, false);
	main_box.pack_start(channels, false, false);
	main_box.pack_start(samplerate, false, false);
	if (metadata){
		main_box.pack_start(fields, true, true);
		main_box.pack_start(top_box, false, false);
	}
	main_box.pack_start(bottom_box, false, false);

	fields.set_size_request(200, 150);

	top_box.set_homogeneous(true);
	top_box.pack_start(add_field_btn);
	top_box.pack_start(remove_field_btn);

	remove_field_btn.set_sensitive(false);

	bottom_box.set_homogeneous(true);
	bottom_box.pack_start(play_btn);
	bottom_box.pack_start(stop_btn);

	play_btn.signal_clicked().connect (mem_fun(*this, &SoundFileBox::play_btn_clicked));
	stop_btn.signal_clicked().connect (mem_fun(*this, &SoundFileBox::stop_btn_clicked));

	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();
	if (!sess) {
		play_btn.set_sensitive(false);
	} else {
		sess->AuditionActive.connect(mem_fun(*this, &SoundFileBox::audition_status_changed));
	}

	add_field_btn.signal_clicked().connect 
			(mem_fun(*this, &SoundFileBox::add_field_clicked));
	remove_field_btn.signal_clicked().connect 
			(mem_fun(*this, &SoundFileBox::remove_field_clicked));

	fields.selection_made.connect (mem_fun(*this, &SoundFileBox::field_selected));
	fields.choice_made.connect (mem_fun(*this, &SoundFileBox::field_chosen));
	
	Library->fields_changed.connect (mem_fun(*this, &SoundFileBox::setup_fields));

	if (setup_labels (uri)) {
		throw failed_constructor();
	}
	
	show_all();
	stop_btn.hide();
}

SoundFileBox::~SoundFileBox ()
{
}

void 
SoundFileBox::_fields_refiller (Gtk::CList &list, void* arg)
{
	((SoundFileBox *) arg)->fields_refiller (list);
}

void
SoundFileBox::fields_refiller (Gtk::CList &clist)
{	
	if (metadata) {
		list<string> flist;
		Library->get_fields(flist);
		list<string>::iterator i;
	
		const gchar *rowdata[3];
		guint row = 0;
		for (i=flist.begin(); i != flist.end(); ++i, ++row){
			if (*i != "channels" && *i != "samplerate" && 
				*i != "resolution" && *i != "format") {

				rowdata[0] = (*i).c_str();
				rowdata[1] = Library->get_field(uri, *i).c_str();
				clist.insert_row (row, rowdata);
				++row;
			}
		}

		clist.column(0).set_auto_resize(true);
		clist.set_sort_column (0);
		clist.sort ();
	}
}

int
SoundFileBox::setup_labels (string uri)
{
	SNDFILE *sf;

	string file;
	if (metadata){
		file = Library->get_member_filename(uri);
	} else {
		file = uri;
	}

	if ((sf = sf_open ((char *) file.c_str(), SFM_READ, sf_info)) < 0) {
		error << compose(_("file \"%1\" could not be opened"), file) << endmsg;
		return -1;
	}
	
	if (sf_info->frames == 0 &&
	    sf_info->channels == 0 &&
	    sf_info->samplerate == 0 &&
	    sf_info->format == 0 &&
	    sf_info->sections == 0) {
		/* .. ok, its not a sound file */
		error << compose(_("file \"%1\" appears not to be an audio file"), file) << endmsg;
		return -1;
	}

	if (metadata) {
		label.set_alignment (0.0f, 0.0f);
		label.set_text ("Label: " + Library->get_label(uri));
	}

	path.set_text ("Path: ");

	path_entry.set_text (file);
	path_entry.set_position (-1);

	path_entry.signal_focus_in_event().connect (ptr_fun (ARDOUR_UI::generic_focus_in_event));
	path_entry.signal_focus_out_event().connect (ptr_fun (ARDOUR_UI::generic_focus_out_event));

	length.set_alignment (0.0f, 0.0f);
	length.set_text (compose("Length: %1", length2string(sf_info->frames, sf_info->samplerate)));

	format.set_alignment (0.0f, 0.0f);
	format.set_text (compose("Format: %1, %2", 
				 sndfile_major_format(sf_info->format), 
				 sndfile_minor_format(sf_info->format)));
	
	channels.set_alignment (0.0f, 0.0f);
	channels.set_text (compose("Channels: %1", sf_info->channels));
	
	samplerate.set_alignment (0.0f, 0.0f);
	samplerate.set_text (compose("Samplerate: %1", sf_info->samplerate));

	return 0;
}

void
SoundFileBox::play_btn_clicked ()
{
	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();
	if (!sess) {
		return;
	}

	sess->cancel_audition();
	string file;

	if (metadata) {
		file = Library->get_member_filename(uri);
	} else {
		file = uri;
	}

	if (access(file.c_str(), R_OK)) {
		warning << compose(_("Could not read file: %1 (%2)."), file, strerror(errno)) << endmsg;
		return;
	}
	
	static map<string, ARDOUR::AudioRegion*> region_cache;

	if (region_cache.find (file) == region_cache.end()) {

		AudioRegion::SourceList srclist;
		SndFileSource* sfs;
		
		for (int n=0; n < sf_info->channels; ++n) {
			
			try {
				sfs =  new SndFileSource(file+":"+compose("%1", n), false);
				srclist.push_back(sfs);
				
			} catch (failed_constructor& err) {
				error << _("Could not access soundfile: ") << file << endmsg;
				return;
			}
		}

		if (srclist.empty()) {
			return;
		}
		
		string result;
		sess->region_name (result, PBD::basename(srclist[0]->name()), false);
		AudioRegion* a_region = new AudioRegion(srclist, 0, srclist[0]->length(), result, 0, Region::DefaultFlags, false);
		region_cache[file] = a_region;
	} 

	play_btn.hide();
	stop_btn.show();

	sess->audition_region(*region_cache[file]);
}

void
SoundFileBox::stop_btn_clicked ()
{
	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();
	if (sess) {
		sess->cancel_audition();
		play_btn.show();
		stop_btn.hide();
	}
}

void
SoundFileBox::audition_status_changed (bool active)
{
	if (!active) {
		Gtkmm2ext::UI::instance()->call_slot( mem_fun(*this, &SoundFileBox::stop_btn_clicked));
	}
}

void
SoundFileBox::add_field_clicked ()
{
	ArdourPrompter prompter (true);
	prompter.set_prompt(_("Field name:"));
	
	prompter.show_all();
	prompter.done.connect(Gtk::Main::quit.slot());
	
	Gtk::Main::run();
	
	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string name;
		
		prompter.get_result(name);
		
		if (name.length()) {
			Library->add_field(name);
		}
	}
}

void
SoundFileBox::remove_field_clicked ()
{
	Library->remove_field(selected_field);
	selected_field = "";
	remove_field_btn.set_sensitive(false);
}

void
SoundFileBox::setup_fields ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &SoundFileBox::setup_fields));
	
	fields.rescan();
}

void
SoundFileBox::field_chosen (Gtkmm2ext::Selector *selector, Gtkmm2ext::SelectionResult *res)
{
	if (res) {
		remove_field_btn.set_sensitive(true);
		selected_field = selector->clist().row(res->row)[0].get_text();
	}
}

void
SoundFileBox::field_selected (Gtkmm2ext::Selector *selector, Gtkmm2ext::SelectionResult *res)
{	
	if (!res){
		return;
	}

	string field_name(selector->clist().row(res->row)[0].get_text());
	
	ArdourPrompter prompter (true);
	prompter.set_prompt(_("Field value:"));
	prompter.set_initial_text (selector->clist().row(res->row)[1].get_text());
	
	prompter.show_all();
	prompter.done.connect(Gtk::Main::quit.slot());

	Gtk::Main::run();
	
	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string data;

		prompter.get_result(data);
		Library->set_field(uri, field_name, data);
	}

	fields.rescan();
}

SearchSounds::SearchSounds ()
	: ArdourDialog ("search sounds dialog"),
	  find_btn (_("Find")),
	  and_rbtn (_("AND")),
	  or_rbtn (_("OR")),
	  fields(_fields_refiller, this, internationalize(selector_titles))
{
	set_title (_("ardour: locate soundfiles"));
	set_name ("AudioSearchSound");

	add(main_box);

	or_rbtn.set_group(and_rbtn.get_group());
	or_rbtn.set_active(true);
	rbtn_box.set_homogeneous(true);
	rbtn_box.pack_start(and_rbtn);
	rbtn_box.pack_start(or_rbtn);

	bottom_box.set_homogeneous(true);
	bottom_box.pack_start(find_btn);

	fields.set_size_request(200, 150);

	main_box.pack_start(fields);
	main_box.pack_start(rbtn_box, false, false);
	main_box.pack_start(bottom_box, false, false);

	delete_event.connect (mem_fun(*this, &ArdourDialog::wm_doi_event));

	find_btn.signal_clicked().connect (mem_fun(*this, &SearchSounds::find_btn_clicked));
	fields.selection_made.connect (mem_fun 
								   (*this, &SearchSounds::field_selected));

	show_all();
}

SearchSounds::~SearchSounds ()
{
}

void 
SearchSounds::_fields_refiller (Gtk::CList &list, void* arg)
{
	((SearchSounds *) arg)->fields_refiller (list);
}

void
SearchSounds::fields_refiller (Gtk::CList &clist)
{	
	list<string> flist;
	Library->get_fields(flist);
	list<string>::iterator i;
	
	const gchar *rowdata[3];
	guint row;
	for (row=0,i=flist.begin(); i != flist.end(); ++i, ++row){
		rowdata[0] = (*i).c_str();
		rowdata[1] = "";
		clist.insert_row (row, rowdata);
	}

	clist.column(0).set_auto_resize(true);
 	clist.set_sort_column (0);
 	clist.sort ();
}

void
SearchSounds::field_selected (Gtkmm2ext::Selector *selector, Gtkmm2ext::SelectionResult *res)
{	
	if (!res){
		return;
	}
	
	ArdourPrompter prompter (true);
	prompter.set_prompt(_("Field value:"));
	
	prompter.show_all();
	prompter.done.connect(Gtk::Main::quit.slot());

	Gtk::Main::run();
	
	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string data;

		prompter.get_result(data);
		selector->clist().cell(res->row, 1).set_text(data);
	}
}

void
SearchSounds::find_btn_clicked ()
{
	using namespace Gtk::CList_Helpers;
	typedef map<string,string> StringMap;

	StringMap search_info;

	RowList rows = fields.clist().rows();
	RowList::const_iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		Cell cfield((*i)[0]);
		Cell cdata((*i)[1]);
		if (cdata.get_text().length()){
			search_info.insert(
				StringMap::value_type(cfield.get_text(), cdata.get_text()));
		}
	}

	SearchResults* results;
	if (and_rbtn.get_active()){
		results = new SearchResults(search_info, true);
	} else {
		results = new SearchResults(search_info, false);
	}

	results->file_chosen.connect (mem_fun(*this, &SearchSounds::file_found));
	results->show_all();
}

void
SearchSounds::file_found (string uri, bool multi)
{
	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();
	if (sess) {
		sess->cancel_audition();
	}

	file_chosen (uri, multi); /* EMIT_SIGNAL */
}

static const gchar* result_titles[] = { 
	N_("Results"), 
	N_("Uris"), 
	0
};

SearchResults::SearchResults (map<string,string> field_values, bool and_search)
	: ArdourDialog ("search results dialog"),
	  search_info(field_values),
	  search_and (and_search),
	  selection (""),
	  main_box (false, 3),
	  import_box (true, 4),
	  import_btn (_("Import")),
	  multichan_check (_("Create multi-channel region")),
	  results (_results_refiller, this, internationalize (result_titles), false, true)
{
	set_title (_("Ardour: Search Results"));
	set_name ("ArdourSearchResults");
	
	add(main_box);
	set_border_width (3);
	
	main_box.pack_start(hbox);
	hbox.pack_start(results);

	main_box.pack_start(import_box, false, false);
	
	results.set_size_request (200, 150);
	
	import_box.set_homogeneous(true);
	import_box.pack_start(import_btn);
	import_box.pack_start(multichan_check);

	import_btn.set_sensitive(false);
	multichan_check.set_active(true);
	multichan_check.set_sensitive(false);
	
	delete_event.connect (mem_fun(*this, &ArdourDialog::wm_doi_event));

	import_btn.signal_clicked().connect (mem_fun(*this, &SearchResults::import_clicked));

	results.choice_made.connect (mem_fun(*this, &SearchResults::result_chosen));

	show_all();
}

SearchResults::~SearchResults ()
{
}

void 
SearchResults::_results_refiller (Gtk::CList &list, void* arg)
{
	((SearchResults *) arg)->results_refiller (list);
}

void 
SearchResults::results_refiller (Gtk::CList &clist)
{
	list<string> results;
	if (search_and) {
		Library->search_members_and (results, search_info);
	} else {
		Library->search_members_or (results, search_info);
	}

	list<string>::iterator i;
	const gchar *rowdata[3];
	guint row;
	for (row=0, i=results.begin(); i != results.end(); ++i, ++row){
		rowdata[0] = (Library->get_label(*i)).c_str();
		// hide the uri in a hidden column
		rowdata[1] = (*i).c_str();
		clist.insert_row (row, rowdata);
	}

	clist.column(1).set_visiblity(false);
	clist.column(0).set_auto_resize(true);
 	clist.set_sort_column (0);
 	clist.sort ();
}

void
SearchResults::import_clicked ()
{
	PublicEditor& edit = ARDOUR_UI::instance()->the_editor();
	ARDOUR::Session* sess = edit.current_session();

	if (sess) {
		sess->cancel_audition();
	}

	file_chosen(selection, multichan_check.get_active()); /* EMIT_SIGNAL */
}

void 
SearchResults::result_chosen (Gtkmm2ext::Selector *selector, Gtkmm2ext::SelectionResult *res)
{
	if (res) {
		selection = selector->clist().row(res->row)[1].get_text();

		if (info_box){
			delete info_box;
			info_box = 0;
		}
	
		try {
			info_box = new SoundFileBox(selection, true);
		} catch (failed_constructor& err) {
			/* nothing to do */
			return;
		}

		hbox.pack_start(*info_box);
	}
}
