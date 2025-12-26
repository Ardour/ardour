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

#pragma once
#include <ytkmm/table.h>

#include "ardour/types.h"

#include "ardour_dialog.h"
#include "public_editor.h"

namespace ARDOUR
{
	class Session;
}

class StripExportDialog : public ArdourDialog
{
public:
	StripExportDialog (PublicEditor&, ARDOUR::Session*);

private:
	void path_changed ();
	void update_sensitivty ();
	void export_strips ();

	ArdourWidgets::ArdourDropdown _what_to_export;
	ArdourWidgets::ArdourDropdown _where_to_export;

	Gtk::Button* _ok_button;
	Gtk::Entry   _name_entry;
	Gtk::Table   _table;

	PublicEditor& _editor;
	std::string   _path;
};
