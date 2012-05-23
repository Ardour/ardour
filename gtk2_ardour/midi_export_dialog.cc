/*
    Copyright (C) 2012 Paul Davis

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

#include "ardour/midi_region.h"

#include "midi_export_dialog.h"

using namespace ARDOUR;

MidiExportDialog::MidiExportDialog (PublicEditor&, boost::shared_ptr<MidiRegion> region)
	: ArdourDialog (string_compose (_("Export MIDI: %1"), region->name()))
	, file_chooser (Gtk::FILE_CHOOSER_ACTION_SAVE)
{
	add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	get_vbox()->pack_start (file_chooser);
	file_chooser.set_filename (region->name() + ".mid");
	file_chooser.show ();
}

MidiExportDialog::~MidiExportDialog ()
{
}

std::string
MidiExportDialog::get_path () const
{
	return file_chooser.get_filename ();
	
}
