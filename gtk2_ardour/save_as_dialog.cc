/*
    Copyright (C) 2015 Paul Davis

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

#include "save_as_dialog.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

SaveAsDialog::SaveAsDialog ()
	: ArdourDialog (_("Save As"))
{
	VBox* vbox = get_vbox();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);
	
	vbox->pack_start (new_name_entry, false, false);
	vbox->pack_start (new_parent_folder_selector, false, false);
	vbox->pack_start (switch_to_button, false, false);
	vbox->pack_start (copy_media_button, false, false);
	vbox->pack_start (copy_external_button, false, false);

	switch_to_button.set_active (true);
	vbox->show_all ();
}

string
SaveAsDialog::new_parent_folder () const
{
	return string();
}

string
SaveAsDialog::new_name () const
{
	return string ();
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
