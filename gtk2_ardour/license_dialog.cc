/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"
#include "license_dialog.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

LicenseDialog::LicenseDialog ()
	: WavesDialog (_("license_dialog.xml"), true, false)
{
	set_modal (true);
	set_resizable (false);
    
	show_all ();
}

void
LicenseDialog::on_esc_pressed ()
{
    hide();
}

void 
LicenseDialog::on_realize ()
{
	WavesDialog::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_ALL));
}

LicenseDialog::~LicenseDialog ()
{
}
