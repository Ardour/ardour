/*
 * Copyright (C) 2008-2009 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>

#include "pbd/failed_constructor.h"

#include "ardour/audio_region_importer.h"
#include "ardour/audio_playlist_importer.h"
#include "ardour/audio_track_importer.h"
#include "ardour/filename_extensions.h"
#include "ardour/location_importer.h"
#include "ardour/tempo_map_importer.h"

#include "gtkmm2ext/utils.h"
#include "widgets/prompter.h"

#include "ardour_message.h"
#include "gui_thread.h"
#include "session_import_dialog.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

SessionImportDialog::SessionImportDialog (ARDOUR::Session* target) :
  ArdourDialog (_("Import from Session")),
  file_browse_button (_("Browse"))
{
	set_session (target);

	// File entry
	file_entry.set_name ("ImportFileNameEntry");
	file_entry.set_text ("/");
	Gtkmm2ext::set_size_request_to_display_given_text (file_entry, X_("Kg/quite/a/reasonable/size/for/files/i/think"), 5, 8);

	file_browse_button.set_name ("EditorGTKButton");
	file_browse_button.signal_clicked().connect (sigc::mem_fun(*this, &SessionImportDialog::browse));

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
	session_tree = TreeStore::create (sb_cols);
	session_browser.set_model (session_tree);

	session_browser.set_name ("SessionBrowser");
	session_browser.append_column (_("Elements"), sb_cols.name);
	session_browser.append_column_editable (_("Import"), sb_cols.queued);
	session_browser.get_column(0)->set_min_width (180);
	session_browser.get_column(1)->set_min_width (40);
	session_browser.get_column(1)->set_sizing (TREE_VIEW_COLUMN_AUTOSIZE);
	if (UIConfiguration::instance().get_use_tooltips()) {
		session_browser.set_tooltip_column (3);
	}

	session_scroll.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	session_scroll.add (session_browser);
	session_scroll.set_size_request (220, 400);

	// Connect signals
	CellRendererToggle *toggle = dynamic_cast<CellRendererToggle *> (session_browser.get_column_cell_renderer (1));
	toggle->signal_toggled().connect(sigc::mem_fun (*this, &SessionImportDialog::update));
	session_browser.signal_row_activated().connect(sigc::mem_fun (*this, &SessionImportDialog::show_info));

	get_vbox()->pack_start (session_scroll, false, false);

	// Buttons
	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	cancel_button->signal_clicked().connect (sigc::mem_fun (*this, &SessionImportDialog::end_dialog));
	ok_button = add_button (_("Import"), RESPONSE_ACCEPT);
	ok_button->signal_clicked().connect (sigc::mem_fun (*this, &SessionImportDialog::do_merge));

	// prompt signals XXX: problem - handlers to be in the same thread since they return values
	ElementImporter::Rename.connect_same_thread (connections, boost::bind (&SessionImportDialog::open_rename_dialog, this, _1, _2));
	ElementImporter::Prompt.connect_same_thread (connections, boost::bind (&SessionImportDialog::open_prompt_dialog, this, _1));

	// Finalize
	show_all();
}

void
SessionImportDialog::load_session (const string& filename)
{
	if (_session) {
		if (tree.read (filename)) {
			error << string_compose (_("Cannot load XML for session from %1"), filename) << endmsg;
			return;
		}
		boost::shared_ptr<AudioRegionImportHandler> region_handler (new AudioRegionImportHandler (tree, *_session));
		boost::shared_ptr<AudioPlaylistImportHandler> pl_handler (new AudioPlaylistImportHandler (tree, *_session, *region_handler));

		handlers.push_back (boost::static_pointer_cast<ElementImportHandler> (region_handler));
		handlers.push_back (boost::static_pointer_cast<ElementImportHandler> (pl_handler));
		handlers.push_back (HandlerPtr(new UnusedAudioPlaylistImportHandler (tree, *_session, *region_handler)));
		handlers.push_back (HandlerPtr(new AudioTrackImportHandler (tree, *_session, *pl_handler)));
		handlers.push_back (HandlerPtr(new LocationImportHandler (tree, *_session)));
		handlers.push_back (HandlerPtr(new TempoMapImportHandler (tree, *_session)));

		fill_list();

		if (ElementImportHandler::dirty()) {
			// Warn user
			string txt = _("Some elements had errors in them. Please see the log for details");
			ArdourMessageDialog msg (txt, false, MESSAGE_WARNING, BUTTONS_OK, true);
			msg.run();
		}
	}
}

void
SessionImportDialog::fill_list ()
{
	session_tree->clear();

	// Loop through element types
	for (HandlerList::iterator handler = handlers.begin(); handler != handlers.end(); ++handler) {
		TreeModel::iterator iter = session_tree->append();
		TreeModel::Row row = *iter;
		row[sb_cols.name] = (*handler)->get_info();
		row[sb_cols.queued] = false;
		row[sb_cols.element] = ElementPtr(); // "Null" pointer

		// Loop through elements
		ElementList &elements = (*handler)->elements;
		for (ElementList::iterator element = elements.begin(); element != elements.end(); ++element) {
			iter = session_tree->append(row.children());
			TreeModel::Row child = *iter;
			child[sb_cols.name] = (*element)->get_name();
			child[sb_cols.queued] = false;
			child[sb_cols.element] = *element;
			child[sb_cols.info] = (*element)->get_info();
		}
	}
}

void
SessionImportDialog::browse ()
{
	FileChooserDialog dialog(_("Import from session"), browse_action());
	dialog.set_transient_for(*this);
	dialog.set_filename (file_entry.get_text());

	FileFilter session_filter;
	session_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	dialog.add_filter (session_filter);
	dialog.set_filter (session_filter);

	dialog.add_button(Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button(Stock::OK, RESPONSE_OK);

	int result = dialog.run();

	if (result == RESPONSE_OK) {
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
	TreeModel::Children types = session_browser.get_model()->children();
	TreeModel::Children::iterator ti;
	for (ti = types.begin(); ti != types.end(); ++ti) {
		// elements
		TreeModel::Children elements = ti->children();
		TreeModel::Children::iterator ei;
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
		ArdourMessageDialog msg (txt, false, MESSAGE_WARNING, BUTTONS_OK, true);
		msg.run();
	}
}


void
SessionImportDialog::update (string path)
{
	TreeModel::iterator cell = session_browser.get_model()->get_iter (path);

	// Select all elements if element type is selected
	if (path.size() == 1) {
		{
			// Prompt user for verification
			string txt = _("This will select all elements of this type!");
			ArdourMessageDialog msg (txt, false, MESSAGE_QUESTION, BUTTONS_OK_CANCEL, true);
			switch (msg.run()) {
				case Gtk::RESPONSE_ACCEPT:
				case Gtk::RESPONSE_OK:
					break;
				default:
					(*cell)[sb_cols.queued] = false;
					return;
			}
		}

		TreeModel::Children elements = cell->children();
		TreeModel::Children::iterator ei;
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
SessionImportDialog::show_info(const TreeModel::Path& path, TreeViewColumn*)
{
	if (path.size() == 1) {
		return;
	}

	TreeModel::iterator cell = session_browser.get_model()->get_iter (path);
	string info = (*cell)[sb_cols.info];

	ArdourMessageDialog msg (info, false, MESSAGE_INFO, BUTTONS_OK, true);
	msg.run();
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
	ArdourWidgets::Prompter prompter(true);
	string new_name;

	prompter.set_name ("Prompter");
	prompter.add_button (Stock::SAVE, RESPONSE_ACCEPT);
	prompter.set_prompt (text);
	prompter.set_initial_text (name);

	if (prompter.run() == RESPONSE_ACCEPT) {
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
	ArdourMessageDialog msg (text, false, MESSAGE_QUESTION, BUTTONS_OK_CANCEL, true);
	if (msg.run() == RESPONSE_OK) {
		return true;
	}
	return false;
}
