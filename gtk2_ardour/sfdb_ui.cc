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

SoundFileBox::SoundFileBox (ARDOUR::Session* session)
	:
	current_pid(0),
	fields(Gtk::ListStore::create(label_columns)),
	main_box (false, 3),
	top_box (true, 4),
	bottom_box (true, 4),
	play_btn(_("Play")),
	stop_btn(_("Stop")),
	add_field_btn(_("Add Field...")),
	remove_field_btn(_("Remove Field"))
{
	set_name (X_("SoundFileBox"));
	border_frame.set_label (_("Soundfile Info"));
	border_frame.add (main_box);

	pack_start (border_frame);
	set_border_width (4);

        path_box.set_spacing (4);
        path_box.pack_start (path, false, false);
        path_box.pack_start (path_entry, true, true);

        main_box.set_border_width (4);

        main_box.pack_start(label, false, false);
        main_box.pack_start(path_box, false, false);
        main_box.pack_start(length, false, false);
        main_box.pack_start(format, false, false);
        main_box.pack_start(channels, false, false);
        main_box.pack_start(samplerate, false, false);
        main_box.pack_start(field_view, true, true);
        main_box.pack_start(top_box, false, false);
        main_box.pack_start(bottom_box, false, false);

        field_view.set_size_request(200, 150);
        top_box.set_homogeneous(true);
        top_box.pack_start(add_field_btn);
        top_box.pack_start(remove_field_btn);

        remove_field_btn.set_sensitive(false);

        bottom_box.set_homogeneous(true);
        bottom_box.pack_start(play_btn);
        bottom_box.pack_start(stop_btn);

        play_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::play_btn_clicked));
        stop_btn.signal_clicked().connect (mem_fun (*this, &SoundFileBox::stop_btn_clicked));

        if (!session) {
                play_btn.set_sensitive(false);
        } else {
                session->AuditionActive.connect(mem_fun (*this, &SoundFileBox::audition_status_changed));
        }

        add_field_btn.signal_clicked().connect
                        (mem_fun (*this, &SoundFileBox::add_field_clicked));
        remove_field_btn.signal_clicked().connect
                        (mem_fun (*this, &SoundFileBox::remove_field_clicked));

        field_view.get_selection()->signal_changed().connect (mem_fun (*this, &SoundFileBox::field_selected));
	ARDOUR::Library->fields_changed.connect (mem_fun (*this, &SoundFileBox::setup_fields));

        show_all();
        stop_btn.hide();
}

int
SoundFileBox::setup_labels (string filename) 
{return 0;}

void
SoundFileBox::setup_fields ()
{}

void
SoundFileBox::play_btn_clicked ()
{}

void
SoundFileBox::stop_btn_clicked ()
{}

void
SoundFileBox::add_field_clicked ()
{}

void
SoundFileBox::remove_field_clicked ()
{}

void
SoundFileBox::audition_status_changed (bool state)
{}

void
SoundFileBox::field_selected ()
{}

bool
SoundFileBox::update (std::string filename)
{return true;}

SoundFileBrowser::SoundFileBrowser (std::string title)
	:
	ArdourDialog(title),
	chooser(Gtk::FILE_CHOOSER_ACTION_OPEN),
	preview(session)
{
	get_vbox()->pack_start(chooser);
	chooser.set_preview_widget(preview);

	chooser.signal_update_preview().connect(mem_fun(*this, &SoundFileBrowser::update_preview));

	show_all();
}

void
SoundFileBrowser::update_preview ()
{
	chooser.set_preview_widget_active(preview.update(chooser.get_filename()));
}

SoundFileChooser::SoundFileChooser (std::string title)
	:
	SoundFileBrowser(title)
{
	add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	show_all();
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

	show_all();
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

