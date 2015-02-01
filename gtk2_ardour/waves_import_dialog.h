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

#ifndef __waves_import_dialog_h__
#define __waves_import_dialog_h__

#include <string>
#include <vector>
#include <map>

#include <sigc++/signal.h>

#include "ardour/audiofilesource.h"
#include "ardour/session_handle.h"

#include "ardour/session.h"
#include "editing.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "waves_dialog.h"

class WavesImportDialog : public WavesDialog
{
public:
	WavesImportDialog (ARDOUR::Session*, uint32_t);
	int run_import ();

protected:

private:
	enum AddingMode {
		AsTrack = 0,
		ToTrack = 1,
		AsRegion = 2,
		AsTapeTrack = 3
	};

	enum InsertionPosition {
		Timestamp = 0,
		EditPoint = 1,
		Playhead = 2,
		Start = 3
	};

	enum ConversionQuality {
		Best = 0,
		Good = 1,
		Quick = 2,
		Fast = 3,
		Fastest = 4
	};

	enum Mapping {
		OneTrackPerFile = 0,
		OneTrackPerChannel = 1,
		MergeFiles = 2,
		SequenceFiles = 3,
		OneRegionPerFile = 4,
		OneRegionPerChannel = 5,
		AllFilesInOneRegion = 6,
		AllFilesInOneTrack = 7
	};


	ARDOUR::SrcQuality _get_src_quality () const;
	Editing::ImportMode _get_import_mode () const;
	Editing::ImportDisposition _get_channel_disposition () const;
	bool _reset_options ();
	bool _check_link_status ();
	bool _check_info (bool&, bool&, bool&);

	void _on_dropdowns (WavesDropdown*, int);
	void _on_cancel_button (WavesButton*);
	void _on_import_button (WavesButton*);
	
	typedef std::map<Mapping, Editing::ImportDisposition> DispositionMap;
	typedef std::pair<Mapping, Editing::ImportDisposition> DispositionMapKey;
	DispositionMap _disposition_map;

    int _status;
    bool _done;
	std::vector<std::string> _files_to_import;
	uint32_t _selected_audio_track_count;
	static std::string __initial_folder;
	
	// UI:
	bool _self_reset;
	const std::string _import_as_track_str;
	const std::string _import_to_track_str;
	const std::string _import_as_region_str;
	const std::string _import_as_tape_track_str;
	const std::string _one_track_per_file_str;
	const std::string _one_track_per_channel_str;
	const std::string _sequence_files_str;
	const std::string _all_files_in_one_track_str;
	const std::string _merge_files_str;
	const std::string _one_region_per_file_str;
	const std::string _one_region_per_channel_str;
	const std::string _all_files_in_one_region_str;

	WavesDropdown& _add_as_dropdown;
	Gtk::Container& _insert_at_home;
	WavesDropdown& _insert_at_dropdown; 
	WavesDropdown& _mapping_dropdown;
	Gtk::Container& _quality_home;
	WavesDropdown& _quality_dropdown;
	Gtk::Container& _copy_to_session_home;
	WavesButton& _copy_to_session_button;
};

#endif // __waves_import_dialog_h__
