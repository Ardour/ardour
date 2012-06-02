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

#ifndef MIDNAM_PATCH_H_
#define MIDNAM_PATCH_H_

#include <iostream>
#include <string>
#include <list>
#include <set>
#include <map>

#include "midi++/event.h"
#include "pbd/xml++.h"

namespace MIDI
{

namespace Name
{

struct PatchPrimaryKey
{
public:
	int msb;
	int lsb;
	int program_number;
	
	PatchPrimaryKey(int a_msb = -1, int a_lsb = -1, int a_program_number = -1) {
		msb = a_msb;
		lsb = a_lsb;
		program_number = a_program_number;
	}
	
	bool is_sane() { 	
		return ((msb >= 0) && (msb <= 127) &&
			(lsb >= 0) && (lsb <= 127) &&
			(program_number >=0 ) && (program_number <= 127));
	}
	
	inline PatchPrimaryKey& operator=(const PatchPrimaryKey& id) {
		msb = id.msb;
		lsb = id.lsb; 
		program_number = id.program_number;
		return *this;
	}
	
	inline bool operator==(const PatchPrimaryKey& id) const {
		return (msb == id.msb && lsb == id.lsb && program_number == id.program_number);
	}
	
	/**
	 * obey strict weak ordering or crash in STL containers
	 */
	inline bool operator<(const PatchPrimaryKey& id) const {
		if (msb < id.msb) {
			return true;
		} else if (msb == id.msb && lsb < id.lsb) {
			return true;
		} else if (msb == id.msb && lsb == id.lsb && program_number < id.program_number) {
			return true;
		}
		
		return false;
	}
};

class PatchBank;
	
class Patch 
{
public:

	Patch (PatchBank* a_bank = 0);
	Patch(std::string a_number, std::string a_name, PatchBank* a_bank = 0);
	virtual ~Patch() {};

	const std::string& name() const          { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	const std::string& number() const        { return _number; }
	void set_number(const std::string a_number)   { _number = a_number; }
	
	const PatchPrimaryKey&   patch_primary_key()   const { return _id; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

	int use_bank_info (PatchBank*);

private:
	std::string        _number;
	std::string        _name;
	PatchPrimaryKey   _id;
};

class PatchBank 
{
public:
	typedef std::list<boost::shared_ptr<Patch> > PatchNameList;

	PatchBank () : _id(0) {};
	PatchBank (std::string a_name, PatchPrimaryKey* an_id = 0) : _name(a_name), _id(an_id) {};
	virtual ~PatchBank() { delete _id; };

	const std::string& name() const               { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	const PatchNameList& patch_name_list() const { return _patch_name_list; }
	const std::string& patch_list_name() const { return _patch_list_name; }

	int set_patch_name_list (const PatchNameList&);

	const PatchPrimaryKey* patch_primary_key()  const { return _id; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string       _name;
	PatchNameList     _patch_name_list;
	PatchPrimaryKey*  _id;
	std::string       _patch_list_name;
};

class ChannelNameSet
{
public:
	typedef std::set<uint8_t>                                    AvailableForChannels;
	typedef std::list<boost::shared_ptr<PatchBank> >             PatchBanks;
	typedef std::map<PatchPrimaryKey, boost::shared_ptr<Patch> > PatchMap;
	typedef std::list<PatchPrimaryKey>                           PatchList;

	ChannelNameSet() {};
	virtual ~ChannelNameSet() {};
	ChannelNameSet(std::string a_name) : _name(a_name) {};

	const std::string& name() const          { return _name; }
	void set_name(const std::string a_name)  { _name = a_name; }
	
	const PatchBanks& patch_banks() const    { return _patch_banks; }

	bool available_for_channel(uint8_t channel) const { 
		return _available_for_channels.find(channel) != _available_for_channels.end(); 
	}
	
	boost::shared_ptr<Patch> find_patch(PatchPrimaryKey& key) {
		assert(key.is_sane());
		return _patch_map[key];
	}
	
	boost::shared_ptr<Patch> previous_patch(PatchPrimaryKey& key) {
		assert(key.is_sane());
		std::cerr << "finding patch with "  << key.msb << "/" << key.lsb << "/" <<key.program_number << std::endl; 
		for (PatchList::const_iterator i = _patch_list.begin();
			 i != _patch_list.end();
			 ++i) {
			if ((*i) == key) {
				if (i != _patch_list.begin()) {
					std::cerr << "got it!" << std::endl;
					--i;
					return  _patch_map[*i];
				} 
			}
		}
			
		return boost::shared_ptr<Patch>();
	}
	
	boost::shared_ptr<Patch> next_patch(PatchPrimaryKey& key) {
		assert(key.is_sane());
		std::cerr << "finding patch with "  << key.msb << "/" << key.lsb << "/" <<key.program_number << std::endl; 
		for (PatchList::const_iterator i = _patch_list.begin();
			 i != _patch_list.end();
			 ++i) {
			if ((*i) == key) {
				if (++i != _patch_list.end()) {
					std::cerr << "got it!" << std::endl;
					return  _patch_map[*i];
				} else {
					--i;
				}
			}
		}
			
		return boost::shared_ptr<Patch>();
	}

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string _name;
	AvailableForChannels _available_for_channels;
	PatchBanks           _patch_banks;
	PatchMap             _patch_map;
	PatchList            _patch_list;
	std::string          _patch_list_name;
};

class Note
{
public:
	Note() {};
	Note(std::string a_number, std::string a_name) : _number(a_number), _name(a_name) {};
	~Note() {};

	const std::string& name() const               { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	const std::string& number() const             { return _number; }
	void set_number(const std::string a_number)   { _number = a_number; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string _number;
	std::string _name;
};

class NoteNameList 
{
public:
	typedef std::list<boost::shared_ptr<Note> > Notes;
	NoteNameList() {};
	NoteNameList (std::string a_name) : _name(a_name) {};
	~NoteNameList() {};

	const std::string& name() const          { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	const Notes& notes() const { return _notes; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string _name;
	Notes  _notes;
};

class CustomDeviceMode
{
public:
	CustomDeviceMode() {};
	virtual ~CustomDeviceMode() {};

	const std::string& name() const          { return _name; }
	void set_name(const std::string a_name)  { _name = a_name; }

	
	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);
	
	std::string channel_name_set_name_by_channel(uint8_t channel) {
		assert(channel <= 15);
		return _channel_name_set_assignments[channel]; 
	}
	
private:
	/// array index = channel number
	/// string contents = name of channel name set 
	std::string _name;
	std::string _channel_name_set_assignments[16];
};

class MasterDeviceNames
{
public:
	typedef std::list<std::string>                                       Models;
	/// maps name to CustomDeviceMode
	typedef std::map<std::string, boost::shared_ptr<CustomDeviceMode> >  CustomDeviceModes;
	typedef std::list<std::string>                                       CustomDeviceModeNames;
	/// maps name to ChannelNameSet
	typedef std::map<std::string, boost::shared_ptr<ChannelNameSet> >    ChannelNameSets;
	typedef std::list<boost::shared_ptr<NoteNameList> >                  NoteNameLists;
	typedef std::map<std::string, PatchBank::PatchNameList>              PatchNameLists;
	
	MasterDeviceNames() {};
	virtual ~MasterDeviceNames() {};
	
	const std::string& manufacturer() const { return _manufacturer; }
	void set_manufacturer(const std::string a_manufacturer) { _manufacturer = a_manufacturer; }
	
	const Models& models() const { return _models; }
	void set_models(const Models some_models) { _models = some_models; }
	
	const CustomDeviceModeNames& custom_device_mode_names() const { return _custom_device_mode_names; }
	
	boost::shared_ptr<CustomDeviceMode> custom_device_mode_by_name(std::string mode_name) {
		assert(mode_name != "");
		return _custom_device_modes[mode_name];
	}
	
	boost::shared_ptr<ChannelNameSet> channel_name_set_by_device_mode_and_channel(std::string mode, uint8_t channel) {
		return _channel_name_sets[custom_device_mode_by_name(mode)->channel_name_set_name_by_channel(channel)];
	}
	
	boost::shared_ptr<Patch> find_patch(std::string mode, uint8_t channel, PatchPrimaryKey& key) {
		return channel_name_set_by_device_mode_and_channel(mode, channel)->find_patch(key);
	}
	
	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);
	
private:
	std::string           _manufacturer;
	Models                _models;
	CustomDeviceModes     _custom_device_modes;
	CustomDeviceModeNames _custom_device_mode_names;
	ChannelNameSets       _channel_name_sets;
	NoteNameLists         _note_name_lists;
	PatchNameLists        _patch_name_lists;
};

class MIDINameDocument
{
public:
	// Maps Model names to MasterDeviceNames
	typedef std::map<std::string, boost::shared_ptr<MasterDeviceNames> > MasterDeviceNamesList;
	
	MIDINameDocument() {}
        MIDINameDocument(const std::string &filename);
	virtual ~MIDINameDocument() {};

	const std::string& author() const { return _author; }
	void set_author(const std::string an_author) { _author = an_author; }
	
	const MasterDeviceNamesList& master_device_names_by_model() const { return _master_device_names_list; }
	
	const MasterDeviceNames::Models& all_models() const { return _all_models; }
		
	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string                   _author;
	MasterDeviceNamesList         _master_device_names_list;
	XMLTree                       _document;
	MasterDeviceNames::Models     _all_models;
};

}

}
#endif /*MIDNAM_PATCH_H_*/
