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

#ifndef MIDI_PATCH_MANAGER_H_
#define MIDI_PATCH_MANAGER_H_

#include <midi++/midnam_patch.h>

namespace ARDOUR {
	class Session;
}

namespace MIDI
{

namespace Name
{

class MidiPatchManager
{
	/// Singleton
private:
	MidiPatchManager() {};
	MidiPatchManager( const MidiPatchManager& );
	MidiPatchManager& operator= (const MidiPatchManager&);
	
	static MidiPatchManager* _manager; 
	
public:
	typedef std::map<std::string, boost::shared_ptr<MIDINameDocument> > MidiNameDocuments;
	
	virtual ~MidiPatchManager() { _manager = 0; }
	
	static MidiPatchManager& instance() { 
		if (_manager == 0) {
			_manager = new MidiPatchManager();
		}
		return *_manager; 
	}
	
	void set_session (ARDOUR::Session&);
	
	boost::shared_ptr<MIDINameDocument> document_by_model(std::string model_name) 
		{ return _documents[model_name]; }
	
	boost::shared_ptr<MasterDeviceNames> master_device_by_model(std::string model_name) 
		{ return _master_devices_by_model[model_name]; }
	
	boost::shared_ptr<Patch> find_patch(
			string model, 
			string custom_device_mode, 
			uint8_t channel, 
			PatchPrimaryKey patch_key) {
		
		boost::shared_ptr<MIDI::Name::MasterDeviceNames> master_device = master_device_by_model(model);
		
		if (master_device != 0 && custom_device_mode != "") {
			return master_device->
			         channel_name_set_by_device_mode_and_channel(custom_device_mode, channel)->
				        find_patch(patch_key);			
		} else {
			return boost::shared_ptr<Patch>();
		}
	}
	
	std::list<string> custom_device_mode_names_by_model(std::string model_name) {
		if (model_name != "") {
			return master_device_by_model(model_name)->custom_device_mode_names();
		} else {
			return std::list<string>();
		}
	}
	
	const MasterDeviceNames::Models& all_models() const { return _all_models; }
	
private:
	void drop_session();
	void refresh();
	
	ARDOUR::Session*                        _session;
	MidiNameDocuments                       _documents;
	MIDINameDocument::MasterDeviceNamesList _master_devices_by_model;
	MasterDeviceNames::Models               _all_models;
};

} // namespace Name

} // namespace MIDI
#endif /* MIDI_PATCH_MANAGER_H_ */
