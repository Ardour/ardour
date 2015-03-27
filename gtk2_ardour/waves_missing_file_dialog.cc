/*
    Copyright (C) 2005-2006 Paul Davis

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
#include "pbd/search_path.h"

#include "ardour/session.h"
#include "ardour/rc_configuration.h"

#include "open_file_dialog.h"
#include "waves_missing_file_dialog.h"
#include "i18n.h"

WavesMissingFileDialog::WavesMissingFileDialog (ARDOUR::Session* session, const std::string& path, ARDOUR::DataType filetype)
  : WavesDialog ("waves_missing_file_dialog.xml", true, false )
  , _filetype (filetype)
  , _additional_folder_name (Glib::get_home_dir())
  , _add_folder_button (get_waves_button ("add_folder_button"))
  , _skip_file_button (get_waves_button ("skip_file_button"))
  , _skip_all_files_button (get_waves_button ("skip_all_files_button"))
  , _stop_loading_button (get_waves_button ("stop_loading_button"))
  , _browse_button (get_waves_button ("browse_button"))
  , _done_button (get_waves_button ("done_button"))
{
	set_session (session);

    std::string typestr;
    switch (_filetype) {
	case ARDOUR::DataType::AUDIO:
            typestr = _("audio");
            break;
    case ARDOUR::DataType::MIDI:
            typestr = _("MIDI");
            break;
    }

	std::vector<std::string> source_dirs = _session->source_search_path (_filetype);
	std::vector<std::string>::iterator i = source_dirs.begin();
	std::ostringstream oss;
	oss << *i << std::endl;

	while (++i != source_dirs.end()) {
		oss << *i << std::endl;
	}

	get_label ("file_type_label").set_text (typestr);
	get_label ("file_name_label").set_text (path);
	get_label ("folder_path_label").set_text (oss.str());
	get_label ("additional_folder_path_label").set_text (_additional_folder_name);

	_add_folder_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_option_button));
	_skip_file_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_option_button));
	_skip_all_files_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_option_button));
	_stop_loading_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_option_button));
	_browse_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_browse_button));
	_done_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMissingFileDialog::_on_done_button));
}

int
WavesMissingFileDialog::get_action ()
{
	if (_add_folder_button.active_state () == Gtkmm2ext::ExplicitActive) {
		_add_chosen ();
		return 0;
	}

	if (_skip_file_button.active_state () == Gtkmm2ext::ExplicitActive) {
		return -1;
	}

	if (_skip_all_files_button.active_state () == Gtkmm2ext::ExplicitActive) {
		return 3;
	}

	return 1;
}

void
WavesMissingFileDialog::_add_chosen ()
{
	std::string str;
	std::string newdir;
	std::vector<std::string> dirs;

	switch (_filetype) {
	case ARDOUR::DataType::AUDIO:
		str = _session->config.get_audio_search_path ();
		break;
	case ARDOUR::DataType::MIDI:
		str = _session->config.get_midi_search_path ();
		break;
	}

	split (str, dirs, G_SEARCHPATH_SEPARATOR);

	for (std::vector<std::string>::iterator d = dirs.begin(); d != dirs.end(); d++) {
		if (*d == _additional_folder_name) {
			return;
		}
	}

	if (!str.empty()) {
		str += G_SEARCHPATH_SEPARATOR;
	}

	str += _additional_folder_name;

	switch (_filetype) {
	case ARDOUR::DataType::AUDIO:
		_session->config.set_audio_search_path (str);
		break;
	case ARDOUR::DataType::MIDI:
		_session->config.set_midi_search_path (str);
		break;
	}
}

void
WavesMissingFileDialog::_on_option_button (WavesButton* button)
{
	_add_folder_button.set_active_state ((button == &_add_folder_button) ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_skip_file_button.set_active_state ((button == &_skip_file_button) ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_skip_all_files_button.set_active_state ((button == &_skip_all_files_button) ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_stop_loading_button.set_active_state ((button == &_stop_loading_button) ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

void
WavesMissingFileDialog::_on_browse_button (WavesButton*)
{
	std::string folder = ARDOUR::choose_folder_dialog (ARDOUR::Config->get_default_session_parent_dir(), _("Select a folder to search"));
	if (!folder.empty ()) {
		_additional_folder_name = folder;
		get_label ("additional_folder_path_label").set_text (_additional_folder_name);
	}
}

void
WavesMissingFileDialog::_on_done_button (WavesButton*)
{
	response (Gtk::RESPONSE_OK);
}
