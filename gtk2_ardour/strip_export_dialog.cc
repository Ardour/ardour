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

#include "ardour/session.h"

#include "strip_export_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

StripExportDialog::StripExportDialog (PublicEditor& editor, Session* s)
	: ArdourDialog (_("Export Track/Bus State"))
{
	set_session (s);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
}

StripExportDialog::~StripExportDialog ()
{
}
