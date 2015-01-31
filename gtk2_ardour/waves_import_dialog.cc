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
#include "ardour/smf_source.h"
#include "ardour/audiofilesource.h"

#include "waves_import_dialog.h"
#include "open_file_dialog_proxy.h"

WavesImportDialog::WavesImportDialog (ARDOUR::Session* session)
  : WavesDialog ("waves_import_dialog.xml", true, false )
  , _add_as_dropdown (get_waves_dropdown ("add_as_dropdown"))
  , _insert_at_dropdown (get_waves_dropdown ("insert_at_dropdown"))
  , _mapping_dropdown (get_waves_dropdown ("mapping_dropdown"))
  , _quality_dropdown (get_waves_dropdown ("quality_dropdown"))
  , _copy_to_session_home (get_container ("copy_to_session_home"))
  , _copy_to_session_button (get_waves_button ("copy_to_session_button"))

{
	set_session (_session);
	_copy_to_session_home.set_visible (!ARDOUR::Config->get_only_copy_imported_files());

	get_waves_button ("import_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_import_button));
	get_waves_button ("cancel_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_cancel_button));
}


ARDOUR::SrcQuality 
WavesImportDialog::_get_src_quality() const
{
	ARDOUR::SrcQuality quality;

	switch (_quality_dropdown.get_item_data_u (_quality_dropdown.get_current_item ())) {
	case Good:
		quality = ARDOUR::SrcGood;
		break;
	case Quick:
		quality = ARDOUR::SrcQuick;
		break;
	case Fast:
		quality = ARDOUR::SrcFast;
		break;
	case Fastest:
		quality = ARDOUR::SrcFastest;
		break;
	case Best:
	default:
		quality = ARDOUR::SrcBest;
		break;
	}
	
	return quality;
}

Editing::ImportMode
WavesImportDialog::_get_import_mode() const
{
	Editing::ImportMode import_mode;
	switch (_add_as_dropdown.get_item_data_u (_add_as_dropdown.get_current_item ())) {
	case AsTrack:
		import_mode = Editing::ImportAsTrack;
		break;
	case ToTrack:
		import_mode = Editing::ImportToTrack;
	case AsRegion:
		import_mode = Editing::ImportAsRegion;
		break;
	case AsTapeTrack:
		import_mode = Editing::ImportAsTapeTrack;
		break;
	default:
		break;
	}
	return import_mode;
}

const std::string __audiofile_types[] = {
	"aif", "AIF", "aifc", "AIFC", "aiff", "AIFF", "amb", "AMB", "au", "AU",	"caf", "CAF",
	"cdr", "CDR", "flac", "FLAC", "htk", "HTK", "iff", "IFF", "mat", "MAT", "oga", "OGA",
	"ogg", "OGG", "paf", "PAF", "pvf", "PVF", "sf", "SF", "smp", "SMP", "snd", "SND",
	"maud", "MAUD",	"voc", "VOC", "vwe", "VWE", "w64", "W64", "wav", "WAV",
#ifdef HAVE_COREAUDIO
	"aac", "AAC", "adts", "ADTS",
	"ac3", "AC3", "amr", "AMR",
	"mpa", "MPA", "mpeg", "MPEG",
	"mp1", "MP1", "mp2", "MP2",
	"mp3", "MP3", "mp4", "MP4",
	"m4a", "M4A", "sd2", "SD2", 	// libsndfile supports sd2 also, but the resource fork is required to open.
#endif // HAVE_COREAUDIO
};

int
WavesImportDialog::run_import ()
{
	std::vector<std::string> audiofile_types (__audiofile_types,
											 __audiofile_types + sizeof (__audiofile_types)/sizeof(__audiofile_types[0]));
	for (;;) {
		_files_to_import = ARDOUR::open_file_dialog (audiofile_types);
		if (_files_to_import.empty ()) {
			return Gtk::RESPONSE_CANCEL;
		}

		bool same_size;
		bool src_needed;
		bool selection_includes_multichannel;
		bool selection_can_be_embedded_with_links = check_link_status ();
		if (check_info (same_size, src_needed, selection_includes_multichannel)) {
				WavesMessageDialog msg ("", string_compose (_("One or more of the selected files\ncannot be used by %1"), PROGRAM_NAME));
				msg.run ();
		} else {
			break;
		}
	}
	return run ();
}

bool
WavesImportDialog::check_link_status ()
{
#ifdef PLATFORM_WINDOWS
	return false;
#else
	std::string tmpdir(Glib::build_filename (_session->session_directory().sound_path(), "linktest"));
	bool ret = false;

	if (mkdir (tmpdir.c_str(), 0744)) {
		if (errno != EEXIST) {
			return false;
		}
	}

	for (vector<string>::const_iterator i = _files_to_import.begin(); i != _files_to_import.end(); ++i) {

		char tmpc[PATH_MAX+1];

		snprintf (tmpc, sizeof(tmpc), "%s/%s", tmpdir.c_str(), Glib::path_get_basename (*i).c_str());

		/* can we link ? */

		if (link ((*i).c_str(), tmpc)) {
			goto out;
		}

		::g_unlink (tmpc);
	}

	ret = true;

  out:
	rmdir (tmpdir.c_str());
	return ret;
#endif
}

bool
WavesImportDialog::check_info (bool& same_size, bool& src_needed, bool& multichannel)
{
	ARDOUR::SoundFileInfo info;
	framepos_t sz = 0;
	bool err = false;
	std::string errmsg;

	same_size = true;
	src_needed = false;
	multichannel = false;

	for (std::vector<std::string>::const_iterator i = _files_to_import.begin(); i != _files_to_import.end(); ++i) {

		if (ARDOUR::AudioFileSource::get_soundfile_info (*i, info, errmsg)) {
			if (info.channels > 1) {
				multichannel = true;
			}
			if (sz == 0) {
				sz = info.length;
			} else {
				if (sz != info.length) {
					same_size = false;
				}
			}

			if (info.samplerate != _session->frame_rate()) {
				src_needed = true;
			}
		} else if (ARDOUR::SMFSource::valid_midi_file (*i)) {
			Evoral::SMF reader;
			reader.open(*i);
			if (reader.num_tracks() > 1) {
				multichannel = true; // "channel" == track here...
			}

			/* XXX we need err = true handling here in case
			   we can't check the file
			*/

		} else {
			err = true;
		}
	}

	return err;
}

void
WavesImportDialog::_on_cancel_button (WavesButton*)
{
	response (Gtk::RESPONSE_CANCEL);
}

void
WavesImportDialog::_on_import_button (WavesButton*)
{
	_done = true;
	_status = Gtk::RESPONSE_OK;
	framepos_t where;

	switch (_insert_at_dropdown.get_item_data_u (_insert_at_dropdown.get_current_item ())) {
	case EditPoint:
		where = PublicEditor::instance().get_preferred_edit_position ();
		break;
	case Timestamp:
		where = -1;
		break;
	case Playhead:
		where = _session->transport_frame();
		break;
	case Start:
	default:
		where = _session->current_start_frame();
		break;
	}

	ARDOUR::SrcQuality quality = _get_src_quality ();

	response (Gtk::RESPONSE_OK);
}
