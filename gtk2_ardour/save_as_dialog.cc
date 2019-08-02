/*
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/stock.h>

#include "ardour/session.h"

#include "gtkmm2ext/utils.h"

#include "save_as_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

SaveAsDialog::SaveAsDialog ()
	: ArdourDialog (_("Save As"))
	, switch_to_button (_("Switch to newly-saved version"))
	, copy_media_button (_("Copy media to new session"))
	, copy_external_button (_("Copy external media into new session"))
	, no_include_media_button (_("Newly-saved session should be empty"))
{
	VBox* vbox = get_vbox();

	vbox->set_spacing (6);

	HBox* hbox;
	Label* label;

	hbox = manage (new HBox);
	hbox->set_spacing (6);
	label = manage (new Label (_("Save as session name")));
	hbox->pack_start (*label, false, false);
	hbox->pack_start (new_name_entry, true, true);
	vbox->pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_spacing (6);
	label = manage (new Label (_("Parent directory/folder")));
	hbox->pack_start (*label, false, false);
	hbox->pack_start (new_parent_folder_selector, true, true);
	vbox->pack_start (*hbox, false, false);

	vbox->pack_start (switch_to_button, false, false);

	VBox* sub_vbox = manage (new VBox);
	HBox* sub_hbox = manage (new HBox);
	HBox* empty = manage (new HBox);

	sub_vbox->pack_start (copy_media_button, false, false);
	sub_vbox->pack_start (copy_external_button, false, false);

	/* indent the two media-related buttons by some amount */
	sub_hbox->set_spacing (24);
	sub_hbox->pack_start (*empty, false, false);
	sub_hbox->pack_start (*sub_vbox, false, false);

	vbox->pack_start (no_include_media_button, false, false);
	vbox->pack_start (*sub_hbox, false, false);

	switch_to_button.set_active (true);
	copy_media_button.set_active (true);

	vbox->show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

	no_include_media_button.signal_toggled ().connect (sigc::mem_fun (*this, &SaveAsDialog::no_include_toggled));

	Gtkmm2ext::add_volume_shortcuts (new_parent_folder_selector);
	new_parent_folder_selector.set_action (FILE_CHOOSER_ACTION_SELECT_FOLDER);
	new_parent_folder_selector.set_current_folder (Config->get_default_session_parent_dir ());

	new_name_entry.signal_changed().connect (sigc::mem_fun (*this, &SaveAsDialog::name_entry_changed));
	new_parent_folder_selector.signal_current_folder_changed().connect (sigc::mem_fun (*this, &SaveAsDialog::name_entry_changed));
	new_parent_folder_selector.signal_selection_changed().connect (sigc::mem_fun (*this, &SaveAsDialog::name_entry_changed));
	set_response_sensitive (RESPONSE_OK, false);
}

void
SaveAsDialog::no_include_toggled ()
{
	if (no_include_media_button.get_active()) {
		copy_media_button.set_sensitive (false);
		copy_external_button.set_sensitive (false);
	} else {
		copy_media_button.set_sensitive (true);
		copy_external_button.set_sensitive (true);
	}
}

void
SaveAsDialog::name_entry_changed ()
{
	if (new_name_entry.get_text().empty()) {
		set_response_sensitive (RESPONSE_OK, false);
		return;
	}

	std::string dir = Glib::build_filename (new_parent_folder(), new_name_entry.get_text());

	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		set_response_sensitive (RESPONSE_OK, false);
		return;
	}

	set_response_sensitive (RESPONSE_OK);
}

string
SaveAsDialog::new_parent_folder () const
{
	return new_parent_folder_selector.get_filename ();
}

string
SaveAsDialog::new_name () const
{
	return new_name_entry.get_text ();
}

bool
SaveAsDialog::switch_to () const
{
	return switch_to_button.get_active ();
}

bool
SaveAsDialog::copy_media () const
{
	return copy_media_button.get_active ();
}

bool
SaveAsDialog::copy_external () const
{
	return copy_external_button.get_active ();
}

void
SaveAsDialog::clear_name ()
{
	new_name_entry.set_text ("");
	set_response_sensitive (RESPONSE_OK, false);
}

void
SaveAsDialog::set_name (std::string name)
{
	new_name_entry.set_text (name);
	name_entry_changed ();
}

bool
SaveAsDialog::include_media () const
{
	return !no_include_media_button.get_active ();
}
