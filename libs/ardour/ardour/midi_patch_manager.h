/*
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef MIDI_PATCH_MANAGER_H_
#define MIDI_PATCH_MANAGER_H_

#include <glibmm/threads.h>

#include "midi++/midnam_patch.h"

#include "pbd/event_loop.h"
#include "pbd/signals.h"
#include "pbd/search_path.h"

#include "ardour/libardour_visibility.h"

namespace MIDI
{

namespace Name
{

class LIBARDOUR_API MidiPatchManager
{
	/// Singleton
private:
	MidiPatchManager();
	MidiPatchManager( const MidiPatchManager& );
	MidiPatchManager& operator= (const MidiPatchManager&);

	static MidiPatchManager* _manager;

public:
	typedef std::map<std::string, boost::shared_ptr<MIDINameDocument> >    MidiNameDocuments;
	typedef std::map<std::string, MIDINameDocument::MasterDeviceNamesList> DeviceNamesByMaker;

        ~MidiPatchManager();

	static MidiPatchManager& instance() {
		if (_manager == 0) {
			_manager = new MidiPatchManager();
		}
		return *_manager;
	}

	PBD::Signal0<void> PatchesChanged;

	bool add_custom_midnam (const std::string& id, char const*);
	bool update_custom_midnam (const std::string& id, char const*);
	bool remove_custom_midnam (const std::string& id);
	bool is_custom_model (const std::string& model) const;

	void add_search_path (const PBD::Searchpath& search_path);

	void remove_search_path (const PBD::Searchpath& search_path);

	boost::shared_ptr<MIDINameDocument> document_by_model(std::string model_name) const;

	boost::shared_ptr<MasterDeviceNames> master_device_by_model(std::string model_name)
		{ return _master_devices_by_model[model_name]; }

	boost::shared_ptr<ChannelNameSet> find_channel_name_set(
			std::string model,
			std::string custom_device_mode,
			uint8_t channel) {
		boost::shared_ptr<MIDI::Name::MasterDeviceNames> master_device = master_device_by_model(model);

		if (master_device != 0 && custom_device_mode != "") {
			return master_device->channel_name_set_by_channel(custom_device_mode, channel);
		} else {
			return boost::shared_ptr<ChannelNameSet>();
		}
	}

	boost::shared_ptr<Patch> find_patch(
			std::string model,
			std::string custom_device_mode,
			uint8_t channel,
			PatchPrimaryKey patch_key) {

		boost::shared_ptr<ChannelNameSet> channel_name_set = find_channel_name_set(model, custom_device_mode, channel);

		if (channel_name_set) {
			return  channel_name_set->find_patch(patch_key);
		} else {
			return boost::shared_ptr<Patch>();
		}
	}

	boost::shared_ptr<Patch> previous_patch(
			std::string model,
			std::string custom_device_mode,
			uint8_t channel,
			PatchPrimaryKey patch_key) {

		boost::shared_ptr<ChannelNameSet> channel_name_set = find_channel_name_set(model, custom_device_mode, channel);

		if (channel_name_set) {
			return  channel_name_set->previous_patch(patch_key);
		} else {
			return boost::shared_ptr<Patch>();
		}
	}

	boost::shared_ptr<Patch> next_patch(
			std::string model,
			std::string custom_device_mode,
			uint8_t channel,
			PatchPrimaryKey patch_key) {

		boost::shared_ptr<ChannelNameSet> channel_name_set = find_channel_name_set(model, custom_device_mode, channel);

		if (channel_name_set) {
			return  channel_name_set->next_patch(patch_key);
		} else {
			return boost::shared_ptr<Patch>();
		}
	}

	std::list<std::string> custom_device_mode_names_by_model(std::string model_name) {
		if (model_name != "") {
			if (master_device_by_model(model_name)) {
				return master_device_by_model(model_name)->custom_device_mode_names();
			}
		}
		return std::list<std::string>();
	}

	const MasterDeviceNames::Models& all_models() const { return _all_models; }

	const DeviceNamesByMaker& devices_by_manufacturer() const { return _devices_by_manufacturer; }

	void load_midnams_in_thread ();
	void maybe_use (PBD::ScopedConnectionList& clist,
	                PBD::EventLoop::InvalidationRecord* ir,
	                const boost::function<void()>& slot,
	                PBD::EventLoop* event_loop);
private:
	bool load_midi_name_document(const std::string& file_path);
	bool add_midi_name_document(boost::shared_ptr<MIDINameDocument>);
	bool remove_midi_name_document(const std::string& file_path, bool emit_signal = true);

	void add_midnam_files_from_directory(const std::string& directory_path);
	void remove_midnam_files_from_directory(const std::string& directory_path);

private:
	PBD::Searchpath                         _search_path;

	MidiNameDocuments                       _documents;
	MIDINameDocument::MasterDeviceNamesList _master_devices_by_model;
	DeviceNamesByMaker                      _devices_by_manufacturer;
	MasterDeviceNames::Models               _all_models;

	Glib::Threads::Mutex _lock;
	bool no_patch_changed_messages;
	bool stop_thread;
	Glib::Threads::Thread* _midnam_load_thread;
	void load_midnams ();
};

} // namespace Name

} // namespace MIDI

#endif /* MIDI_PATCH_MANAGER_H_ */
