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

#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/midi_patch_manager.h"

#include "ardour/midi_patch_search_path.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace MIDI::Name;
using namespace PBD;

MidiPatchManager* MidiPatchManager::_manager = 0;

MidiPatchManager::MidiPatchManager ()
{
	refresh ();
}

void
MidiPatchManager::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	add_session_patches ();
}

void
MidiPatchManager::add_session_patches ()
{
	if (!_session) {
		return;
	}
	
	std::string path_to_patches = _session->session_directory().midi_patch_path();

	if (!Glib::file_test (path_to_patches, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	assert (Glib::file_test (path_to_patches, Glib::FILE_TEST_IS_DIR));

	Glib::PatternSpec pattern(string("*.midnam"));
	vector<std::string> result;

	find_matching_files_in_directory (path_to_patches, pattern, result);

	info << "Loading " << result.size() << " MIDI patches from " << path_to_patches << endmsg;

	for (vector<std::string>::iterator i = result.begin(); i != result.end(); ++i) {
		boost::shared_ptr<MIDINameDocument> document(new MIDINameDocument(*i));
		for (MIDINameDocument::MasterDeviceNamesList::const_iterator device =
					document->master_device_names_by_model().begin();
				device != document->master_device_names_by_model().end();
				++device) {
			//cerr << "got model " << device->first << endl;
			// have access to the documents by model name
			_documents[device->first] = document;
			// build a list of all master devices from all documents
			_master_devices_by_model[device->first] = device->second;
			_all_models.insert(device->first);

			// make sure there are no double model names
			// TODO: handle this gracefully.
			assert(_documents.count(device->first) == 1);
			assert(_master_devices_by_model.count(device->first) == 1);
		}
	}
}

void
MidiPatchManager::refresh()
{
	_documents.clear();
	_master_devices_by_model.clear();
	_all_models.clear();

	Searchpath search_path = midi_patch_search_path ();
	Glib::PatternSpec pattern (string("*.midnam"));
	vector<std::string> result;

	find_matching_files_in_search_path (search_path, pattern, result);

	info << "Loading " << result.size() << " MIDI patches from " << search_path.to_string() << endmsg;

	for (vector<std::string>::iterator i = result.begin(); i != result.end(); ++i) {
		boost::shared_ptr<MIDINameDocument> document;
		try {
			document = boost::shared_ptr<MIDINameDocument>(new MIDINameDocument(*i));
		} catch (...) {
			error << "Error parsing MIDI patch file " << *i << endmsg;
			continue;
		}
		for (MIDINameDocument::MasterDeviceNamesList::const_iterator device =
			     document->master_device_names_by_model().begin();
		     device != document->master_device_names_by_model().end();
		     ++device) {
			if (_documents.find(device->first) != _documents.end()) {
				warning << string_compose(_("Duplicate MIDI device `%1' in `%2' ignored"),
				                          device->first, *i)
				        << endmsg;
				continue;
			}
				                      
			_documents[device->first]               = document;
			_master_devices_by_model[device->first] = device->second;

			_all_models.insert(device->first);
		}
	}

	if (_session) {
		add_session_patches ();
	}
}

void
MidiPatchManager::session_going_away ()
{
	SessionHandlePtr::session_going_away ();
	refresh ();
}
