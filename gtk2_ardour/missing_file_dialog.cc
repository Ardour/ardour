/*
    Copyright (C) 2010 Paul Davis

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

#include "pbd/compose.h"
#include "pbd/replace_all.h"
#include "pbd/strsplit.h"

#include "ardour/session.h"

#include "missing_file_dialog.h"

using namespace Gtk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

MissingFileDialog::MissingFileDialog (Session* s, const std::string& path, DataType type)
        : ArdourDialog (_("Missing File!"), true, false)
        , filetype (type)
        , chooser (FILE_CHOOSER_ACTION_SELECT_FOLDER)
        , use_chosen (_("Add chosen folder to search path, and try again"))
        , choice_group (use_chosen.get_group())
        , use_chosen_and_no_more_questions (choice_group, _("Add chosen folder to search path, try again but don't ask me again"), false)
        , stop_loading_button (choice_group, _("Stop loading this session"), false)
        , all_missing_ok (choice_group, _("This and all other missing files are OK"), false)
        , this_missing_ok (choice_group, _("This missing file is OK"), false)
{
        set_session (s);

        add_button (_("Done"), RESPONSE_OK);

        string typestr;

        switch (type) {
        case DataType::AUDIO:
                typestr = _("An audio");
                break;
        case DataType::MIDI:
                typestr = _("A MIDI");
                break;
        }

        string dirstr;

        dirstr = s->source_search_path (type);
        replace_all (dirstr, ":", "\n");

        msg.set_markup (string_compose (_("%1 file (\"%2\") cannot be found.\n\n\
Currently, Ardour has searched in the following folders for this file:\n\n\
<tt>%3</tt>\n\n\
The following options are available:"), typestr, path, dirstr));

        VBox* button_packer_box = manage (new VBox);

        button_packer_box->set_spacing (6);
        button_packer_box->set_border_width (12);

        button_packer_box->pack_start (use_chosen, false, false);
        button_packer_box->pack_start (use_chosen_and_no_more_questions, false, false);
        button_packer_box->pack_start (this_missing_ok, false, false);
        button_packer_box->pack_start (all_missing_ok, false, false);
        button_packer_box->pack_start (stop_loading_button, false, false);

        button_packer_box->show_all ();
        
        get_vbox()->set_spacing (6);
        get_vbox()->set_border_width (12);
        get_vbox()->set_homogeneous (false);

        get_vbox()->pack_start (msg, false, false);

        HBox* hbox = manage (new HBox);
        hbox->pack_start (*button_packer_box, false, true);
        hbox->show ();

        get_vbox()->pack_start (*hbox, false, false);
        get_vbox()->pack_start (chooser, true, true);

        msg.show ();
        chooser.set_size_request (-1, 300);
        chooser.show ();
        chooser.set_current_folder (Glib::get_home_dir());
        chooser.set_create_folders (false);
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

        split (str, dirs, ':');

        newdir = chooser.get_filename ();

        for (vector<string>::iterator d = dirs.begin(); d != dirs.end(); d++) {
                if (*d == newdir) {
                        return;
                }
        }

        if (!str.empty()) {
                str += ':';
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
                add_chosen ();
                return 0;
        }

        if (use_chosen_and_no_more_questions.get_active()) {
                add_chosen ();
                return 2;
        }

        if (this_missing_ok.get_active()) {
                return -1;
        }

        if (all_missing_ok.get_active ()) {
                return 3;
        }

        return 1;
}
