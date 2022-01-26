/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"
#include "pbd/replace_all.h"
#include "pbd/strsplit.h"
#include "pbd/search_path.h"

#include "ardour/session.h"
#include "gtkmm2ext/utils.h"

#include "missing_file_dialog.h"
#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

MissingFileDialog::MissingFileDialog (Gtk::Window& parent, Session* s, const std::string& path, DataType type)
	: ArdourDialog (parent, _("Missing File"), true, false)
	, filetype (type)
	, is_absolute_path (Glib::path_is_absolute (path))
	, chooser (_("Select a folder to search"), FILE_CHOOSER_ACTION_SELECT_FOLDER)
	, use_chosen (_("Add chosen folder to search path, and try again"))
	, choice_group (use_chosen.get_group())
	, stop_loading_button (choice_group, _("Stop loading this session"), false)
	, all_missing_ok (choice_group, _("Skip all missing files"), false)
	, this_missing_ok (choice_group, _("Skip this file"), false)
{
	/* This dialog is always shown programmatically. Center the window.*/
	set_position (Gtk::WIN_POS_CENTER);

	set_session (s);

	add_button (_("Done"), RESPONSE_OK);
	set_default_response (RESPONSE_OK);

	string typestr;

	switch (type) {
		case DataType::AUDIO:
			typestr = _("audio");
			break;
		case DataType::MIDI:
			typestr = _("MIDI");
			break;
	}

	vector<string> source_dirs = s->source_search_path (type);
	vector<string>::iterator i = source_dirs.begin();
	ostringstream oss;
	oss << *i << endl;

	while (++i != source_dirs.end()) {
		oss << *i << endl;
	}

	msg.set_justify (JUSTIFY_LEFT);
	msg.set_markup (string_compose (_("%1 cannot find the %2 file\n\n<i>%3</i>\n\nin any of these folders:\n\n\
					<tt>%4</tt>\n\n"), PROGRAM_NAME, typestr, Gtkmm2ext::markup_escape_text (path), Gtkmm2ext::markup_escape_text (oss.str())));

	HBox* hbox = manage (new HBox);
	hbox->pack_start (msg, false, true);
	hbox->show ();

	get_vbox()->pack_start (*hbox, false, false);

	VBox* button_packer_box = manage (new VBox);

	button_packer_box->set_spacing (6);
	button_packer_box->set_border_width (12);

	button_packer_box->pack_start (use_chosen, false, false);
	button_packer_box->pack_start (this_missing_ok, false, false);
	button_packer_box->pack_start (all_missing_ok, false, false);
	button_packer_box->pack_start (stop_loading_button, false, false);

	button_packer_box->show_all ();

	get_vbox()->set_spacing (6);
	get_vbox()->set_border_width (25);
	get_vbox()->set_homogeneous (false);


	hbox = manage (new HBox);
	hbox->pack_start (*button_packer_box, false, true);
	hbox->show ();

	get_vbox()->pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	Label* label = manage (new Label);
	label->set_text (_("Click to choose an additional folder"));

	hbox->set_spacing (6);
	hbox->set_border_width (12);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (chooser, true, true);
	hbox->show_all ();

	get_vbox()->pack_start (*hbox, true, true);

	msg.show ();

	Gtkmm2ext::add_volume_shortcuts (chooser);
	chooser.set_current_folder (Glib::get_home_dir());
	chooser.set_create_folders (false);
}

void
MissingFileDialog::set_absolute ()
{
	_session->set_missing_file_replacement (chooser.get_filename ());
}

void
MissingFileDialog::add_chosen ()
{
	string str;
	string newdir;
	vector<string> dirs;

	switch (filetype) {
		case DataType::AUDIO:
			str = _session->config.get_audio_search_path();
			break;
		case DataType::MIDI:
			str = _session->config.get_midi_search_path();
			break;
	}

	split (str, dirs, G_SEARCHPATH_SEPARATOR);

	newdir = chooser.get_filename ();

	for (vector<string>::iterator d = dirs.begin(); d != dirs.end(); d++) {
		if (*d == newdir) {
			return;
		}
	}

	if (!str.empty()) {
		str += G_SEARCHPATH_SEPARATOR;
	}

	str += newdir;

	switch (filetype) {
		case DataType::AUDIO:
			_session->config.set_audio_search_path (str);
			break;
		case DataType::MIDI:
			_session->config.set_midi_search_path (str);
			break;
	}
}

int
MissingFileDialog::get_action ()
{
	if (use_chosen.get_active ()) {
		if (is_absolute_path) {
			set_absolute ();
		} else {
			add_chosen ();
		}
		return 0;
	}

	if (this_missing_ok.get_active()) {
		return -1;
	}

	if (all_missing_ok.get_active ()) {
		return 3;
	}

	return 1;
}
