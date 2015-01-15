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

#include "waves_export_dialog.h"

void
WavesExportDialog::init ()
{
    _export_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::_on_export_button_clicked));
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::_on_export_button_clicked));
}

void
WavesExportDialog::_on_export_button_clicked (WavesButton*)
{
	response (Gtk::RESPONSE_OK);
}

void
WavesExportDialog::_on_cancel_button_clicked (WavesButton*)
{
	response (Gtk::RESPONSE_CANCEL);
}