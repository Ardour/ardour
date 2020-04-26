/*
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include <boost/shared_ptr.hpp>

#include <glibmm/fileutils.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/unwind.h"

#include "ardour/midi_patch_manager.h"

#include "ardour/search_paths.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace MIDI;
using namespace MIDI::Name;
using namespace PBD;

MidiPatchManager* MidiPatchManager::_manager = 0;

MidiPatchManager::MidiPatchManager ()
	: no_patch_changed_messages (false)
	, stop_thread (false)
{
	add_search_path (midi_patch_search_path ());
}

MidiPatchManager::~MidiPatchManager ()
{
	_manager = 0;

	stop_thread = true;
	_midnam_load_thread->join ();
}

void
MidiPatchManager::add_search_path (const Searchpath& search_path)
{
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
	}
}

bool
MidiPatchManager::add_custom_midnam (const std::string& id, char const* midnam)
{
	boost::shared_ptr<MIDINameDocument> document;
	document = boost::shared_ptr<MIDINameDocument>(new MIDINameDocument());
	XMLTree mxml;
	if (mxml.read_buffer (midnam, true)) {
		if (0 == document->set_state (mxml, *mxml.root())) {
			document->set_file_path ("custom:" + id);
			add_midi_name_document (document);
			return true;
		}
	}
	return false;
}

bool
MidiPatchManager::remove_custom_midnam (const std::string& id)
{
	return remove_midi_name_document ("custom:" + id);
}

bool
MidiPatchManager::update_custom_midnam (const std::string& id, char const* midnam)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	remove_midi_name_document ("custom:" + id, false);
	return add_custom_midnam (id, midnam);
}

bool
MidiPatchManager::is_custom_model (const std::string& model) const
{
	boost::shared_ptr<MIDINameDocument> midnam = document_by_model (model);
	return (midnam && midnam->file_path().substr(0, 7) == "custom:");
}

void
MidiPatchManager::add_midnam_files_from_directory(const std::string& directory_path)
{
	vector<std::string> result;
	find_files_matching_pattern (result, directory_path, "*.midnam");

	info << string_compose (P_("Loading %1 MIDI patch from %2", "Loading %1 MIDI patches from %2", result.size()), result.size(), directory_path) << endmsg;

	for (vector<std::string>::const_iterator i = result.begin(); i != result.end(); ++i) {
		if (stop_thread) {
			break;
		}
		load_midi_name_document (*i);
	}
}

void
MidiPatchManager::remove_search_path (const Searchpath& search_path)
{
	for (Searchpath::const_iterator i = search_path.begin(); i != search_path.end(); ++i) {

		if (!_search_path.contains(*i)) {
			continue;
		}

		remove_midnam_files_from_directory(*i);

		_search_path.remove_directory (*i);
	}
}

void
MidiPatchManager::remove_midnam_files_from_directory(const std::string& directory_path)
{
	vector<std::string> result;
	find_files_matching_pattern (result, directory_path, "*.midnam");

	info << string_compose(
		P_("Unloading %1 MIDI patch from %2", "Unloading %1 MIDI patches from %2", result.size()),
		result.size(), directory_path)
	     << endmsg;

	for (vector<std::string>::const_iterator i = result.begin(); i != result.end(); ++i) {
		remove_midi_name_document (*i);
	}
}

bool
MidiPatchManager::load_midi_name_document (const std::string& file_path)
{
	boost::shared_ptr<MIDINameDocument> document;
	try {
		document = boost::shared_ptr<MIDINameDocument>(new MIDINameDocument(file_path));
	}
	catch (...) {
		error << string_compose(_("Error parsing MIDI patch file %1"), file_path)
		      << endmsg;
		return false;
	}
	return add_midi_name_document (document);
}

boost::shared_ptr<MIDINameDocument>
MidiPatchManager::document_by_model(std::string model_name) const
{
	MidiNameDocuments::const_iterator i = _documents.find (model_name);
	if (i != _documents.end ()) {
		return i->second;
	}
	return boost::shared_ptr<MIDINameDocument> ();
}

bool
MidiPatchManager::add_midi_name_document (boost::shared_ptr<MIDINameDocument> document)
{
	bool added = false;
	for (MIDINameDocument::MasterDeviceNamesList::const_iterator device =
		     document->master_device_names_by_model().begin();
	     device != document->master_device_names_by_model().end();
	     ++device) {
		if (_documents.find(device->first) != _documents.end()) {
			warning << string_compose(_("Duplicate MIDI device `%1' in `%2' ignored"),
			                          device->first,
			                          document->file_path()) << endmsg;
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

		added = true;
		// TODO: handle this gracefully.
		assert(_documents.count(device->first) == 1);
		assert(_master_devices_by_model.count(device->first) == 1);
	}

	if (added && !no_patch_changed_messages) {
		PatchesChanged(); /* EMIT SIGNAL */
	}

	return added;
}

bool
MidiPatchManager::remove_midi_name_document (const std::string& file_path, bool emit_signal)
{
	bool removed = false;
	for (MidiNameDocuments::iterator i = _documents.begin(); i != _documents.end();) {
		if (i->second->file_path() == file_path) {

			boost::shared_ptr<MIDINameDocument> document = i->second;

			info << string_compose(_("Removing MIDI patch file %1"), file_path) << endmsg;

			_documents.erase(i++);

			for (MIDINameDocument::MasterDeviceNamesList::const_iterator device =
				     document->master_device_names_by_model().begin();
			     device != document->master_device_names_by_model().end();
			     ++device) {

				_master_devices_by_model.erase(device->first);

				_all_models.erase(device->first);

				const std::string& manufacturer = device->second->manufacturer();

				_devices_by_manufacturer[manufacturer].erase(device->first);
			}
			removed = true;
		} else {
			++i;
		}
	}
	if (removed && emit_signal) {
		PatchesChanged(); /* EMIT SIGNAL */
	}
	return removed;
}

void
MidiPatchManager::load_midnams ()
{
	/* really there's only going to be one x-thread request/signal before
	   this thread exits but we'll say 8 just to be sure.
	*/

	PBD::notify_event_loops_about_thread_creation (pthread_self(), "midi-patch-manager", 8);
	pthread_set_name ("MIDNAMLoader");

	{
		PBD::Unwinder<bool> npc (no_patch_changed_messages, true);
		for (Searchpath::const_iterator i = _search_path.begin(); i != _search_path.end(); ++i) {
			Glib::Threads::Mutex::Lock lm (_lock);
			add_midnam_files_from_directory (*i);
		}
	}

	PatchesChanged (); /* EMIT SIGNAL */
}

void
MidiPatchManager::load_midnams_in_thread ()
{
	_midnam_load_thread = Glib::Threads::Thread::create (sigc::mem_fun (*this, &MidiPatchManager::load_midnams));
}

void
MidiPatchManager::maybe_use (PBD::ScopedConnectionList& cl,
                             PBD::EventLoop::InvalidationRecord* ir,
                             const boost::function<void()> & midnam_info_method,
                             PBD::EventLoop* event_loop)
{
	{
		Glib::Threads::Mutex::Lock lm (_lock);

		if (!_documents.empty()) {
			/* already have documents loaded, so call closure to use them */
			midnam_info_method ();
		}

		/* if/when they ever change, call the closure (maybe multiple times) */

		PatchesChanged.connect (cl, ir, midnam_info_method, event_loop);
	}
}
