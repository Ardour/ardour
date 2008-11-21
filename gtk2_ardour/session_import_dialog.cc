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

#include "session_import_dialog.h"

#include <pbd/failed_constructor.h>

#include <ardour/audio_region_importer.h>
#include <ardour/audio_playlist_importer.h>
#include <ardour/audio_track_importer.h>
#include <ardour/location_importer.h>
#include <ardour/tempo_map_importer.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "prompter.h"
#include "i18n.h"

using namespace ARDOUR;

SessionImportDialog::SessionImportDialog (ARDOUR::Session & target) :
  ArdourDialog (_("Import from session")),
  target (target),
  file_browse_button (_("Browse"))
{
	// File entry
	file_entry.set_name ("ImportFileNameEntry");
	file_entry.set_text ("/");
	Gtkmm2ext::set_size_request_to_display_given_text (file_entry, X_("Kg/quite/a/reasonable/size/for/files/i/think"), 5, 8);
	
	file_browse_button.set_name ("EditorGTKButton");
	file_browse_button.signal_clicked().connect (mem_fun(*this, &SessionImportDialog::browse));
	
	file_hbox.set_spacing (5);
	file_hbox.set_border_width (5);
	file_hbox.pack_start (file_entry, true, true);
	file_hbox.pack_start (file_browse_button, false, false);
	
	file_frame.add (file_hbox);
	file_frame.set_border_width (5);
	file_frame.set_name ("ImportFrom");
	file_frame.set_label (_("Import from Session"));
	
	get_vbox()->pack_start (file_frame, false, false);

	// Session browser
	session_tree = Gtk::TreeStore::create (sb_cols);
	session_browser.set_model (session_tree);

	session_browser.set_name ("SessionBrowser");
	session_browser.append_column (_("Elements"), sb_cols.name);
	session_browser.append_column_editable (_("Import"), sb_cols.queued);
	session_browser.get_column(0)->set_min_width (180);
	session_browser.get_column(1)->set_min_width (40);
	session_browser.get_column(1)->set_sizing (Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
	
	session_scroll.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	session_scroll.add (session_browser);
	session_scroll.set_size_request (220, 400);
	
	// Connect signals
	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *> (session_browser.get_column_cell_renderer (1));
	toggle->signal_toggled().connect(mem_fun (*this, &SessionImportDialog::update));
	session_browser.signal_row_activated().connect(mem_fun (*this, &SessionImportDialog::show_info));
	
	get_vbox()->pack_start (session_scroll, false, false);
	
	// Tooltips
	session_browser.set_has_tooltip();
	session_browser.signal_query_tooltip().connect(mem_fun(*this, &SessionImportDialog::query_tooltip));
	
	// Buttons
	cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	cancel_button->signal_clicked().connect (mem_fun (*this, &SessionImportDialog::end_dialog));
	ok_button = add_button (_("Import"), Gtk::RESPONSE_ACCEPT);
	ok_button->signal_clicked().connect (mem_fun (*this, &SessionImportDialog::do_merge));
	
	// prompt signals
	ElementImporter::Rename.connect (mem_fun (*this, &SessionImportDialog::open_rename_dialog));
	ElementImporter::Prompt.connect (mem_fun (*this, &SessionImportDialog::open_prompt_dialog));
	
	// Finalize
	show_all();
}

void
SessionImportDialog::load_session (const string& filename)
{
	tree.read (filename);
	AudioRegionImportHandler *region_handler;
	
	region_handler = new AudioRegionImportHandler (tree, target);
	handlers.push_back (HandlerPtr(region_handler));
	handlers.push_back (HandlerPtr(new AudioPlaylistImportHandler (tree, target, *region_handler)));
	handlers.push_back (HandlerPtr(new UnusedAudioPlaylistImportHandler (tree, target, *region_handler)));
	handlers.push_back (HandlerPtr(new AudioTrackImportHandler (tree, target)));
	handlers.push_back (HandlerPtr(new LocationImportHandler (tree, target)));
	handlers.push_back (HandlerPtr(new TempoMapImportHandler (tree, target)));
	
	fill_list();
	
	if (ElementImportHandler::dirty()) {
		// Warn user
		string txt = _("Some elements had errors in them. Please see the log for details");
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
		msg.run();
	}
}

void
SessionImportDialog::fill_list ()
{
	session_tree->clear();
	
	// Loop through element types
	for (HandlerList::iterator handler = handlers.begin(); handler != handlers.end(); ++handler) {
		Gtk::TreeModel::iterator iter = session_tree->append();
		Gtk::TreeModel::Row row = *iter;
		row[sb_cols.name] = (*handler)->get_info();
		row[sb_cols.queued] = false;
		row[sb_cols.element] = ElementPtr(); // "Null" pointer
		
		// Loop through elements
		ElementList &elements = (*handler)->elements;
		for (ElementList::iterator element = elements.begin(); element != elements.end(); ++element) {
			iter = session_tree->append(row.children());
			Gtk::TreeModel::Row child = *iter;
			child[sb_cols.name] = (*element)->get_name();
			child[sb_cols.queued] = false;
			child[sb_cols.element] = *element;
		}
	}
}

void
SessionImportDialog::browse ()
{
	Gtk::FileChooserDialog dialog(_("Import from session"), browse_action());
	dialog.set_transient_for(*this);
	dialog.set_filename (file_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
  
	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		string filename = dialog.get_filename();
	
		if (filename.length()) {
			file_entry.set_text (filename);
			load_session (filename);
		}
	}
}

void
SessionImportDialog::do_merge ()
{
	
	// element types
	Gtk::TreeModel::Children types = session_browser.get_model()->children();
	Gtk::TreeModel::Children::iterator ti;
	for (ti = types.begin(); ti != types.end(); ++ti) {
		// elements
		Gtk::TreeModel::Children elements = ti->children();
		Gtk::TreeModel::Children::iterator ei;
		for (ei = elements.begin(); ei != elements.end(); ++ei) {
			if ((*ei)[sb_cols.queued]) {
				ElementPtr element = (*ei)[sb_cols.element];
				element->move();
			}
		}
	}
	
	end_dialog();
	
	if (ElementImportHandler::errors()) {
		// Warn user
		string txt = _("Some elements had errors in them. Please see the log for details");
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
		msg.run();
	}
}


void
SessionImportDialog::update (string path)
{
	Gtk::TreeModel::iterator cell = session_browser.get_model()->get_iter (path);
	
	// Select all elements if element type is selected
	if (path.size() == 1) {
		{
			// Prompt user for verification
			string txt = _("This will select all elements of this type!");
			Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL, true);
			if (msg.run() == Gtk::RESPONSE_CANCEL) {
				(*cell)[sb_cols.queued] = false;
				return;
			}
		}
		
		Gtk::TreeModel::Children elements = cell->children();
		Gtk::TreeModel::Children::iterator ei;
		for (ei = elements.begin(); ei != elements.end(); ++ei) {
			ElementPtr element = (*ei)[sb_cols.element];
			if (element->prepare_move()) {
				(*ei)[sb_cols.queued] = true;
			} else {
				(*cell)[sb_cols.queued] = false; // Not all are selected
			}
		}
		return;
	}
	
	ElementPtr element = (*cell)[sb_cols.element];
	if ((*cell)[sb_cols.queued]) {
		if (!element->prepare_move()) {
			(*cell)[sb_cols.queued] = false;
		}
	} else {
		element->cancel_move();
	}
}

void
SessionImportDialog::show_info(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column)
{
	if (path.size() == 1) {
		return;
	}
	
	Gtk::TreeModel::iterator cell = session_browser.get_model()->get_iter (path);
	ElementPtr element = (*cell)[sb_cols.element];
	string info = element->get_info();
	
	Gtk::MessageDialog msg (info, false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
	msg.run();
}

bool
SessionImportDialog::query_tooltip(int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
	Gtk::TreeModel::Path path;
	Gtk::TreeViewColumn* column;
	int cell_x, cell_y;
	
	// Get element
	session_browser.get_path_at_pos (x, y, path, column, cell_x, cell_y);
	if (path.gobj() == 0) {
		return false;
	}
	Gtk::TreeModel::iterator row = session_browser.get_model()->get_iter (path);
	//--row; // FIXME Strange offset in rows, if someone figures this out, please fix
	ElementPtr element = (*row)[sb_cols.element];
	if (element.get() == 0) {
		return false;
	}
	
	// Prepare tooltip
	tooltip->set_text(element->get_info());
	
	return true;
}

void
SessionImportDialog::end_dialog ()
{
	hide_all();

	set_modal (false);
	ok_button->set_sensitive(true);
}

std::pair<bool, string>
SessionImportDialog::open_rename_dialog (string text, string name)
{
	ArdourPrompter prompter(true);
	string new_name;

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_prompt (text);
	prompter.set_initial_text (name);
	
	if (prompter.run() == Gtk::RESPONSE_ACCEPT) {
		prompter.get_result (new_name);
		if (new_name.length()) {
			name = new_name;
		}
		return std::make_pair (true, new_name);
	}
	return std::make_pair (false, new_name);
}

bool
SessionImportDialog::open_prompt_dialog (string text)
{
	Gtk::MessageDialog msg (text, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL, true);
	if (msg.run() == Gtk::RESPONSE_OK) {
		return true;
	}
	return false;
}
