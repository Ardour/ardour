/*
    Copyright (C) 2015 Paul Davis
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>

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

#include <gtkmm/stock.h>

#include "ardour/session.h"

#include "session_archive_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

SessionArchiveDialog::SessionArchiveDialog ()
	: ArdourDialog (_("Zip/Archive Session"))
	, ProgressReporter ()
{
	VBox* vbox = get_vbox();

	vbox->set_spacing (6);

	HBox* hbox;
	Label* label;

	zip_ext.append_text (".tar.xz");
	zip_ext.set_active_text (".tar.xz");

	hbox = manage (new HBox);
	hbox->set_spacing (6);
	label = manage (new Label (_("Archive Name")));
	hbox->pack_start (*label, false, false);
	hbox->pack_start (name_entry, true, true);
	hbox->pack_start (zip_ext, false, false);
	vbox->pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_spacing (6);
	label = manage (new Label (_("Target directory/folder")));
	hbox->pack_start (*label, false, false);
	hbox->pack_start (target_folder_selector, true, true);
	vbox->pack_start (*hbox, false, false);

	vbox->pack_start (progress_bar, true, true, 12);
	progress_bar.set_text (_("Archiving"));

	vbox->show_all ();
	progress_bar.hide ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

	target_folder_selector.set_action (FILE_CHOOSER_ACTION_SELECT_FOLDER);
	target_folder_selector.set_current_folder (Config->get_default_session_parent_dir ()); // TODO get/set default_archive_dir
	name_entry.signal_changed().connect (sigc::mem_fun (*this, &SessionArchiveDialog::name_entry_changed));
	target_folder_selector.signal_current_folder_changed().connect (sigc::mem_fun (*this, &SessionArchiveDialog::name_entry_changed));
	target_folder_selector.signal_selection_changed().connect (sigc::mem_fun (*this, &SessionArchiveDialog::name_entry_changed));
	set_response_sensitive (RESPONSE_OK, false);
}


void
SessionArchiveDialog::name_entry_changed ()
{
	if (name_entry.get_text().empty()) {
		set_response_sensitive (RESPONSE_OK, false);
		return;
	}

	std::string dir = Glib::build_filename (target_folder(), name_entry.get_text());

	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		set_response_sensitive (RESPONSE_OK, false);
		return;
	}

	set_response_sensitive (RESPONSE_OK);
}

string
SessionArchiveDialog::target_folder () const
{
	return target_folder_selector.get_filename ();
}

string
SessionArchiveDialog::name () const
{
	return name_entry.get_text ();
}

void
SessionArchiveDialog::set_name (std::string name)
{
	name_entry.set_text (name);
	name_entry_changed ();
}

void
SessionArchiveDialog::update_progress_gui (float p)
{
	set_response_sensitive (RESPONSE_OK, false);
	set_response_sensitive (RESPONSE_CANCEL, false);
	progress_bar.show ();
	progress_bar.set_fraction (p);
}
