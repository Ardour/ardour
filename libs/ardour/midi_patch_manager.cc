/*
    Copyright (C) 2008 Hans Baier

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

    $Id$
*/

#include <boost/shared_ptr.hpp>

#include <glibmm/fileutils.h>

#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "ardour/midi_patch_manager.h"

#include "ardour/search_paths.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace MIDI::Name;
using namespace PBD;

MidiPatchManager* MidiPatchManager::_manager = 0;

MidiPatchManager::MidiPatchManager ()
{
	add_search_path(midi_patch_search_path ());
}

void
MidiPatchManager::add_search_path (const Searchpath& search_path)
{
	bool do_refresh = false;

	for (Searchpath::const_iterator i = search_path.begin(); i != search_path.end(); ++i) {

		if (_search_path.contains(*i)) {
			// already processed files from this path
			continue;
		}

		if (!Glib::file_test (*i, Glib::FILE_TEST_EXISTS)) {
			continue;
		}

		if (!Glib::file_test (*i, Glib::FILE_TEST_IS_DIR)) {
			continue;
		}

		_search_path.add_directory (*i);
		do_refresh = true;
	}

	if (do_refresh) {
		refresh();
	}
}

void
MidiPatchManager::remove_search_path (const Searchpath& search_path)
{
	bool do_refresh = false;

	for (Searchpath::const_iterator i = search_path.begin(); i != search_path.end(); ++i) {

		if (!_search_path.contains(*i)) {
			continue;
		}

		_search_path.remove_directory (*i);
		do_refresh = true;
	}

	if (do_refresh) {
		refresh();
	}
}

bool
MidiPatchManager::add_midi_name_document (const std::string& file_path)
{
	boost::shared_ptr<MIDINameDocument> document;
	try {
		document = boost::shared_ptr<MIDINameDocument>(new MIDINameDocument(file_path));
	}
	catch (...) {
		error << "Error parsing MIDI patch file " << file_path << endmsg;
		return false;
	}
	for (MIDINameDocument::MasterDeviceNamesList::const_iterator device =
	         document->master_device_names_by_model().begin();
	     device != document->master_device_names_by_model().end();
	     ++device) {
		if (_documents.find(device->first) != _documents.end()) {
			warning << string_compose(_("Duplicate MIDI device `%1' in `%2' ignored"),
			                          device->first,
			                          file_path) << endmsg;
			continue;
		}

		_documents[device->first] = document;
		_master_devices_by_model[device->first] = device->second;

		_all_models.insert(device->first);
		const std::string& manufacturer = device->second->manufacturer();
		if (_devices_by_manufacturer.find(manufacturer) ==
		    _devices_by_manufacturer.end()) {
			MIDINameDocument::MasterDeviceNamesList empty;
			_devices_by_manufacturer.insert(std::make_pair(manufacturer, empty));
		}
		_devices_by_manufacturer[manufacturer].insert(
		    std::make_pair(device->first, device->second));

		// TODO: handle this gracefully.
		assert(_documents.count(device->first) == 1);
		assert(_master_devices_by_model.count(device->first) == 1);
	}
	return true;
}

void
MidiPatchManager::refresh()
{
	_documents.clear();
	_master_devices_by_model.clear();
	_all_models.clear();
	_devices_by_manufacturer.clear();

	vector<std::string> result;

	find_files_matching_pattern (result, _search_path, "*.midnam");

	info << "Loading " << result.size() << " MIDI patches from "
	     << _search_path.to_string() << endmsg;

	for (vector<std::string>::iterator i = result.begin(); i != result.end(); ++i) {
		add_midi_name_document (*i);
	}
}
