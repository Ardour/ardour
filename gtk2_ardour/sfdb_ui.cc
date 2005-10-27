/*
    Copyright (C) 2005 Paul Davis 
    Written by Taybin Rutkin

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

#include <gtkmm/box.h>
#include <gtkmm/stock.h>

#include <ardour/audio_library.h>

#include "sfdb_ui.h"

#include "i18n.h"

SoundFileBrowser::SoundFileBrowser (std::string title)
	:
	Gtk::Dialog(title, false),
	chooser(Gtk::FILE_CHOOSER_ACTION_OPEN)
{
	get_vbox()->pack_start(chooser);
}

SoundFileChooser::SoundFileChooser (std::string title)
	:
	SoundFileBrowser(title)
{
	add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
}

SoundFileOmega::SoundFileOmega (std::string title)
	:
	SoundFileBrowser(title),
	embed_btn (_("Embed")),
	import_btn (_("Import")),
	split_check (_("Split Channels"))
{
	get_action_area()->pack_start(embed_btn);
	get_action_area()->pack_start(import_btn);
	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);

	chooser.set_extra_widget(split_check);

	embed_btn.signal_clicked().connect (mem_fun (*this, &SoundFileOmega::embed_clicked));
	import_btn.signal_clicked().connect (mem_fun (*this, &SoundFileOmega::import_clicked));
}

void
SoundFileOmega::embed_clicked ()
{
	Embedded (chooser.get_filenames(), split_check.get_active());
}

void
SoundFileOmega::import_clicked ()
{
	Imported (chooser.get_filenames(), split_check.get_active());
}

