/*
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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
#include <gtkmm/table.h>

#include "ardour/filename_extensions.h"

#include "gtkmm2ext/utils.h"

#include "session_archive_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

SessionArchiveDialog::SessionArchiveDialog ()
	: ArdourDialog (_("Zip/Archive Current Session"))
	, ProgressReporter ()
	, only_used_checkbox (_("Exclude unused audio sources"))
{
	VBox* vbox = get_vbox();

	vbox->set_spacing (6);

	format_selector.append_text (ARDOUR::session_archive_suffix);
	format_selector.set_active_text (ARDOUR::session_archive_suffix);

	encode_selector.append_text (_("None"));
	encode_selector.append_text (_("FLAC 16bit"));
	encode_selector.append_text (_("FLAC 24bit"));
	encode_selector.set_active_text (_("FLAC 16bit")); // TODO remember

	compression_selector.append_text (_("None"));
	compression_selector.append_text (_("Fast"));
	compression_selector.append_text (_("Good"));
	compression_selector.set_active_text (_("Good")); // TODO remember

	Gtk::Table* table = manage (new Gtk::Table ());
	table->set_col_spacings (10);
	table->set_row_spacings (8);

	Label* label;
	int row = 0;

	label = manage (new Label (_("Archive Name:"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	table->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	HBox* hbox = manage (new HBox);
	hbox->set_spacing (6);
	hbox->pack_start (name_entry, true, true);
	hbox->pack_start (format_selector, false, false);
	table->attach (*hbox, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	++row;

	label = manage (new Label (_("Target directory/folder:"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	table->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	table->attach (target_folder_selector, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	++row;

	label = manage (new Label (_("Audio Compression:"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	table->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	table->attach (encode_selector, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	++row;

	label = manage (new Label (_("Archive Compression:"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	table->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	table->attach (compression_selector, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	++row;

	table->attach (only_used_checkbox, 0, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	++row;

	label = manage (new Label (_("Note: This archives only the current session state, snapshots are not included."), ALIGN_START));
	label->set_line_wrap (true);
	table->attach (*label, 0, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

	vbox->pack_start (*table, false, false);
	vbox->pack_start (progress_bar, true, true, 12);

	vbox->show_all ();
	progress_bar.hide ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

	Gtkmm2ext::add_volume_shortcuts (target_folder_selector);
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

	std::string dir = Glib::build_filename (target_folder(), name_entry.get_text() + format_selector.get_active_text ());

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

void
SessionArchiveDialog::set_target_folder (const std::string& name)
{
	target_folder_selector.set_current_folder (name);
	name_entry_changed ();
}

string
SessionArchiveDialog::name () const
{
	return name_entry.get_text ();
}

void
SessionArchiveDialog::set_name (const std::string& name)
{
	name_entry.set_text (name);
	name_entry_changed ();
}

bool
SessionArchiveDialog::only_used_sources () const
{
	return only_used_checkbox.get_active ();
}

void
SessionArchiveDialog::set_only_used_sources (bool en)
{
	only_used_checkbox.set_active (en);
}

ARDOUR::Session::ArchiveEncode
SessionArchiveDialog::encode_option () const
{
	string codec = encode_selector.get_active_text ();
	if (codec == _("FLAC 16bit")) {
		return ARDOUR::Session::FLAC_16BIT;
	}
	if (codec == _("FLAC 24bit")) {
		return ARDOUR::Session::FLAC_24BIT;
	}
	return ARDOUR::Session::NO_ENCODE;
}

void
SessionArchiveDialog::set_encode_option (ARDOUR::Session::ArchiveEncode e)
{
	switch (e) {
		case ARDOUR::Session::FLAC_16BIT:
			encode_selector.set_active_text (_("FLAC 16bit"));
			break;
		case ARDOUR::Session::FLAC_24BIT:
			encode_selector.set_active_text (_("FLAC 24bit"));
			break;
		default:
			encode_selector.set_active_text (_("None"));
			break;
	}
}

PBD::FileArchive::CompressionLevel
SessionArchiveDialog::compression_level () const
{
	string codec = compression_selector.get_active_text ();
	if (codec == _("Fast")) {
		return PBD::FileArchive::CompressFast;
	} else if (codec == _("None")) {
		return PBD::FileArchive::CompressNone;
	}
	return PBD::FileArchive::CompressGood;
}

void
SessionArchiveDialog::set_compression_level (PBD::FileArchive::CompressionLevel l)
{
	switch (l) {
		case PBD::FileArchive::CompressFast:
			encode_selector.set_active_text (_("Fast"));
			break;
		case PBD::FileArchive::CompressNone:
			encode_selector.set_active_text (_("None"));
			break;
		case PBD::FileArchive::CompressGood:
			encode_selector.set_active_text (_("Good"));
			break;
	}
}

void
SessionArchiveDialog::update_progress_gui (float p)
{
	set_response_sensitive (RESPONSE_OK, false);
	set_response_sensitive (RESPONSE_CANCEL, false);
	progress_bar.show ();
	if (p < 0) {
		progress_bar.set_text (_("Archiving Session"));
		return;
	}
	if (p > 1.0) {
		progress_bar.set_text (_("Encoding Audio"));
		return;
	}
	progress_bar.set_fraction (p);
}
