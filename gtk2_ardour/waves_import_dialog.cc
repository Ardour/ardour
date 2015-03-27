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
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>

#include "ardour/smf_source.h"
#include "ardour/audiofilesource.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"

#include "waves_import_dialog.h"
#include "open_file_dialog.h"
#include "audio_time_axis.h"

std::string WavesImportDialog::__initial_folder;

WavesImportDialog::WavesImportDialog (ARDOUR::Session* session, uint32_t selected_audio_track_count)
  : WavesDialog ("waves_import_dialog.xml", true, false )
  , _add_as_dropdown (get_waves_dropdown ("add_as_dropdown"))
  , _status (0)
  , _done (false)
  , _selected_audio_track_count (selected_audio_track_count)
  , _insert_at_home (get_container ("insert_at_home"))
  , _insert_at_dropdown (get_waves_dropdown ("insert_at_dropdown"))
  , _mapping_dropdown (get_waves_dropdown ("mapping_dropdown"))
  , _quality_home (get_container ("quality_home"))
  , _quality_dropdown (get_waves_dropdown ("quality_dropdown"))
  , _copy_to_session_home (get_container ("copy_to_session_home"))
  , _copy_to_session_button (get_waves_button ("copy_to_session_button"))
  , _self_reset (false)
  , _import_as_track_str (xml_property (*xml_tree ()->root (), "ImportAsTrack", "???"))
  , _import_to_track_str (xml_property (*xml_tree ()->root (), "ImportToTrack", "???"))
  , _import_as_region_str (xml_property (*xml_tree ()->root (), "ImportAsRegion", "???"))
  , _import_as_tape_track_str (xml_property (*xml_tree ()->root (), "ImportAsTapeTrack", "???"))
  , _one_track_per_file_str (xml_property (*xml_tree ()->root (), "OneTrackPerFile", "???"))
  , _one_track_per_channel_str (xml_property (*xml_tree ()->root (), "OneTrackPerChannel", "???"))
  , _sequence_files_str (xml_property (*xml_tree ()->root (), "SequenceFiles", "???"))
  , _all_files_in_one_track_str (xml_property (*xml_tree ()->root (), "AllFilesInOneTrack", "???"))
  , _merge_files_str (xml_property (*xml_tree ()->root (), "MergeFiles", "???"))
  , _one_region_per_file_str (xml_property (*xml_tree ()->root (), "OneRegionPerFile", "???"))
  , _one_region_per_channel_str (xml_property (*xml_tree ()->root (), "OneRegionPerChannel", "???"))
  , _all_files_in_one_region_str (xml_property (*xml_tree ()->root (), "AllFilesInOneRegion", "???"))
{
	set_session (session);
	_disposition_map.insert (DispositionMapKey (OneTrackPerFile, Editing::ImportDistinctFiles));
	_disposition_map.insert (DispositionMapKey (OneTrackPerChannel, Editing::ImportDistinctChannels));
	_disposition_map.insert (DispositionMapKey (MergeFiles, Editing::ImportMergeFiles));
	_disposition_map.insert (DispositionMapKey (SequenceFiles, Editing::ImportSerializeFiles));

	_disposition_map.insert (DispositionMapKey (OneRegionPerFile, Editing::ImportDistinctFiles));
	_disposition_map.insert (DispositionMapKey (OneRegionPerChannel, Editing::ImportDistinctChannels));
	_disposition_map.insert (DispositionMapKey (AllFilesInOneRegion, Editing::ImportMergeFiles));
	_disposition_map.insert (DispositionMapKey (AllFilesInOneTrack, Editing::ImportMergeFiles));
	
	_insert_at_dropdown.set_current_item (0);
	_mapping_dropdown.set_current_item (0);
    _quality_dropdown.set_current_item (0);

	get_waves_button ("import_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_import_button));
	get_waves_button ("cancel_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_cancel_button));
	_add_as_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_dropdowns));
	_mapping_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_dropdowns));
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
        break;
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
	do {
		_files_to_import = ARDOUR::open_file_dialog (audiofile_types, true, __initial_folder);
		if (_files_to_import.empty ()) {
			return Gtk::RESPONSE_CANCEL;
		}
		__initial_folder = Glib::path_get_dirname (_files_to_import[0]);
	} while (!_reset_options ());
	return run ();
}

Editing::ImportDisposition
WavesImportDialog::_get_channel_disposition () const
{
	/* we use a map here because the channel combo can contain different strings
	   depending on the state of the other combos. the map contains all possible strings
	   and the ImportDisposition enum that corresponds to it.
	*/

	Mapping mapping = Mapping(_mapping_dropdown.get_item_data_i (_mapping_dropdown.get_current_item ()));
	DispositionMap::const_iterator x = _disposition_map.find (mapping);

	if (x == _disposition_map.end()) {
		PBD::fatal << string_compose (_("programming error: %1 (%2)"), "unknown value for import disposition", mapping) << endmsg;
		/*NOTREACHED*/
	}

	return x->second;
}

bool
WavesImportDialog::_reset_options ()
{
	bool same_size;
	bool src_needed;
	bool selection_includes_multichannel;
	bool selection_can_be_embedded_with_links = _check_link_status ();

	if (_check_info (same_size, src_needed, selection_includes_multichannel)) {
			WavesMessageDialog msg ("", string_compose (_("One or more of the selected files\ncannot be used by %1"), PROGRAM_NAME));
			msg.run ();
			return false;
	}

	_copy_to_session_home.set_visible (!ARDOUR::Config->get_only_copy_imported_files ());

	_self_reset = true;
	int current_add_as_mode = AddingMode(_add_as_dropdown.get_item_data_i (_add_as_dropdown.get_current_item ()));
	_add_as_dropdown.clear_items ();
	_add_as_dropdown.add_menu_item (_import_as_track_str, (void*)AsTrack);
//	_add_as_dropdown.add_menu_item (_import_as_tape_track_str, (void*)AsTapeTrack);

	if (_selected_audio_track_count > 0) {
		if (_mapping_dropdown.get_current_item () >= 0) {
			switch (_get_channel_disposition ()) {
			case Editing::ImportDistinctFiles:
				if (_selected_audio_track_count == _files_to_import.size ()) {
					_add_as_dropdown.add_menu_item (_import_to_track_str, (void*)ToTrack);
				}
				break;
					
			case Editing::ImportDistinctChannels:
				/* XXX it would be nice to allow channel-per-selected track
					but its too hard we don't want to deal with all the
					different per-file + per-track channel configurations.
				*/
				break;
					
			default:
				_add_as_dropdown.add_menu_item (_import_to_track_str, (void*)ToTrack);
				break;
			}
		}
	}

	if (current_add_as_mode >= 0) {
		int size = _add_as_dropdown.get_menu ().items ().size ();
		int i;
		for (i = 0; i < size; i++) {
			if (_add_as_dropdown.get_item_data_i (i) == current_add_as_mode) {
				_add_as_dropdown.set_current_item (i);
				break;
			}
		}
		if (i == size) {
			_add_as_dropdown.set_current_item (0);
		}
	} else {
		_add_as_dropdown.set_current_item (0);
	}

	int current_mapping = _mapping_dropdown.get_item_data_i (_mapping_dropdown.get_current_item ());
	_mapping_dropdown.clear_items ();
	Editing::ImportMode mode = _get_import_mode ();
	_insert_at_home.set_visible (mode != Editing::ImportAsRegion);

	if ((mode == Editing::ImportAsTrack) || (mode == Editing::ImportAsTapeTrack) || (mode == Editing::ImportToTrack)) {
		_mapping_dropdown.add_menu_item (_one_track_per_file_str, (void*)OneTrackPerFile);

		if (selection_includes_multichannel) {
			_mapping_dropdown.add_menu_item (_one_track_per_channel_str, (void*)OneTrackPerChannel);
		}

		if (_files_to_import.size() > 1) {
			/* tape tracks are a single region per track, so we cannot
			   sequence multiple files.
			*/
			if (mode != Editing::ImportAsTapeTrack) {
				_mapping_dropdown.add_menu_item (_sequence_files_str, (void*)SequenceFiles);
			}
			if (same_size) {
				_mapping_dropdown.add_menu_item (_all_files_in_one_track_str, (void*)AllFilesInOneTrack);
				_mapping_dropdown.add_menu_item (_merge_files_str, (void*)MergeFiles);
			}
		}
	} else {
		_mapping_dropdown.add_menu_item (_one_region_per_file_str, (void*)OneRegionPerFile);

		if (selection_includes_multichannel) {
			_mapping_dropdown.add_menu_item (_one_region_per_channel_str, (void*)OneRegionPerChannel);
		}

		if (_files_to_import.size() > 1) {
			if (same_size) {
				_mapping_dropdown.add_menu_item (_all_files_in_one_region_str, (void*)AllFilesInOneRegion);
			}
		}
	}

	/* preserve any existing choice, if possible */
	if (current_mapping >= 0) {
		int size = _mapping_dropdown.get_menu ().items ().size ();
		int i;
		for (i = 0; i < size; i++) {
			if (_mapping_dropdown.get_item_data_i (i) == current_mapping) {
				_mapping_dropdown.set_current_item (i);
				break;
			}
		}
		if (i == size) {
			_mapping_dropdown.set_current_item (0);
		}
	} else {
		_mapping_dropdown.set_current_item (0);
	}

	_self_reset = false;

	_quality_home.set_visible (src_needed);
	return true;
}


bool
WavesImportDialog::_check_link_status ()
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

	for (std::vector<std::string>::const_iterator i = _files_to_import.begin(); i != _files_to_import.end(); ++i) {

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
WavesImportDialog::_check_info (bool& same_size, bool& src_needed, bool& multichannel)
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
            if (info.channels > 2 ) {
                err = true;
            }
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
			err = true;
		} else {
			err = true;
		}
	}

	return err;
}

void
WavesImportDialog::_on_dropdowns (WavesDropdown*, int)
{
	if (!_self_reset) {
		_reset_options ();
	}
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

	switch (_insert_at_dropdown.get_item_data_i (_insert_at_dropdown.get_current_item ())) {
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

	Editing::ImportMode import_mode = _get_import_mode ();
	Editing::ImportDisposition channel_disposition = _get_channel_disposition ();
	ARDOUR::SrcQuality quality = _get_src_quality ();
	hide ();
	if (_copy_to_session_button.active_state () == Gtkmm2ext::ExplicitActive) {
		PublicEditor::instance().do_import (_files_to_import, channel_disposition, import_mode, quality, where);
	} else {
		PublicEditor::instance().do_embed (_files_to_import, channel_disposition, import_mode, where);
	}

	response (Gtk::RESPONSE_OK);
}
