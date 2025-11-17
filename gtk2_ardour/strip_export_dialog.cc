/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include <ytkmm/stock.h>

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session.h"

#include "strip_export_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

StripExportDialog::StripExportDialog (PublicEditor& editor, Session* s)
	: ArdourDialog (_("Export Track/Bus State"))
	, _editor (editor)
{
	set_session (s);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_ok_button = manage (new Button (Stock::OK));
	get_action_area ()->pack_end (*_ok_button);

	_what_to_export.append_text_item (_("Complete Session"));
	if (!_editor.get_selection ().tracks.empty ()) {
		_what_to_export.append_text_item (_("Selected Tracks/Busses"));
	}
	_what_to_export.set_active (0);

	_where_to_export.append_text_item (_("Local (Session Folder)"));
	_where_to_export.append_text_item (_("Global (Config Folder)"));
	_where_to_export.set_active (0);

	_where_to_export.StateChanged.connect (sigc::mem_fun (*this, &StripExportDialog::path_changed));
	_name_entry.signal_changed ().connect (sigc::mem_fun (*this, &StripExportDialog::path_changed));

	_table.set_spacings (3);
	/* clang-format: off */
	_table.attach (*manage (new Label (_("What to export:"))), 0, 1, 0, 1, Gtk::FILL, Gtk::SHRINK);
	_table.attach (*manage (new Label (_("Export as:"))),      0, 1, 1, 2, Gtk::FILL, Gtk::SHRINK);
	_table.attach (*manage (new Label (_("Name:"))),           0, 1, 2, 3, Gtk::FILL, Gtk::SHRINK);

	_table.attach (_what_to_export,  1, 2, 0, 1, Gtk::FILL, Gtk::SHRINK);
	_table.attach (_where_to_export, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK);
	_table.attach (_name_entry,      1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK);
	/* clang-format: on */

	get_vbox ()->pack_start (_table, false, false);

	_ok_button->show ();
	_ok_button->set_sensitive (false);
	_ok_button->signal_clicked ().connect (mem_fun (*this, &StripExportDialog::export_strips), false);

	_table.show_all ();
}

void
StripExportDialog::path_changed ()
{
	string name  = legalize_for_path (_name_entry.get_text ());
	bool   local = _where_to_export.get_active_row_number () == 0;
	bool   ok    = false;

	if (name.empty ()) {
		_path = "";
		goto out;
	}

	if (local) {
		_path = Glib::build_filename (_session->path (), routestates_dir_name, name);
	} else {
		_path = Glib::build_filename (user_config_directory (), routestates_dir_name, name);
	}

	ok = !Glib::file_test (_path, Glib::FileTest (G_FILE_TEST_EXISTS));

out:
	_ok_button->set_sensitive (ok);
}

void
StripExportDialog::export_strips ()
{
	std::shared_ptr<RouteList> rl (new RouteList);
	if (_what_to_export.get_active_row_number () == 0) {
		RouteList const& rlx (*_session->get_routes ());
		std::copy (rlx.begin (), rlx.end (), std::back_inserter (*rl));
	} else {
		for (auto const& r : _editor.get_selection ().tracks.routelist ()) {
			rl->push_back (r);
		}
	}

	int rv = _session->export_route_state (rl, _path, false) ? RESPONSE_ACCEPT : RESPONSE_REJECT;
	ArdourDialog::on_response (rv);
}
