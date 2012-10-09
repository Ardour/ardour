/*
    Copyright (C) 2012 Paul Davis
    Author: Hans Baier

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

#include <algorithm>
#include <iostream>
#include <string>
#include <list>
#include <set>
#include <map>

#include <stdint.h>

#include "midi++/event.h"
#include "pbd/xml++.h"

namespace MIDI
{

namespace Name
{

struct PatchPrimaryKey
{
public:
	int bank_number;
	int program_number;

		PatchPrimaryKey (uint8_t a_program_number = 0, uint16_t a_bank_number = 0) {
		bank_number = std::min (a_bank_number, (uint16_t) 16384);
		program_number = std::min (a_program_number, (uint8_t) 127);
	}
	
	bool is_sane() { 	
		return ((bank_number >= 0) && (bank_number <= 16384) && 
			(program_number >=0 ) && (program_number <= 127));
	}
	
	inline PatchPrimaryKey& operator=(const PatchPrimaryKey& id) {
		bank_number = id.bank_number;
		program_number = id.program_number;
		return *this;
	}
	
	inline bool operator==(const PatchPrimaryKey& id) const {
		return (bank_number == id.bank_number && program_number == id.program_number);
	}
	
	/**
	 * obey strict weak ordering or crash in STL containers
	 */
	inline bool operator<(const PatchPrimaryKey& id) const {
		if (bank_number < id.bank_number) {
			return true;
		} else if (bank_number == id.bank_number && program_number < id.program_number) {
			return true;
		}
		
		return false;
	}
};

class PatchBank;
	
class Patch 
{
public:

	Patch (std::string a_name = std::string(), uint8_t a_number = 0, uint16_t bank_number = 0);
	virtual ~Patch() {};

	const std::string& name() const          { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	uint8_t program_number() const       { return _id.program_number; }
	void set_program_number(uint8_t n)   { _id.program_number = n; }

	uint16_t bank_number() const       { return _id.bank_number; }
	void set_bank_number (uint16_t n) { _id.bank_number = n; }

	const PatchPrimaryKey&   patch_primary_key()   const { return _id; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string     _name;
	PatchPrimaryKey _id;
};

class PatchBank 
{
public:
	typedef std::list<boost::shared_ptr<Patch> > PatchNameList;

	PatchBank (uint16_t n = 0, std::string a_name = std::string()) : _name(a_name), _number (n) {};
	virtual ~PatchBank() { }

	const std::string& name() const               { return _name; }
	void set_name(const std::string a_name)       { _name = a_name; }

	int number() const { return _number; }

	const PatchNameList& patch_name_list() const { return _patch_name_list; }
	const std::string& patch_list_name() const { return _patch_list_name; }

	int set_patch_name_list (const PatchNameList&);

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string       _name;
	uint16_t          _number;
	PatchNameList     _patch_name_list;
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
		for (PatchList::const_iterator i = _patch_list.begin();
			 i != _patch_list.end();
			 ++i) {
			if ((*i) == key) {
				if (i != _patch_list.begin()) {
					--i;
					return  _patch_map[*i];
				} 
			}
		}
			
		return boost::shared_ptr<Patch>();
	}
	
	boost::shared_ptr<Patch> next_patch(PatchPrimaryKey& key) {
		assert(key.is_sane());
		for (PatchList::const_iterator i = _patch_list.begin();
			 i != _patch_list.end();
			 ++i) {
			if ((*i) == key) {
				if (++i != _patch_list.end()) {
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

	void set_patch_banks (const PatchBanks&);
	void use_patch_name_list (const PatchBank::PatchNameList&);

private:
	friend std::ostream& operator<< (std::ostream&, const ChannelNameSet&);
	std::string _name;
	AvailableForChannels _available_for_channels;
	PatchBanks           _patch_banks;
	PatchMap             _patch_map;
	PatchList            _patch_list;
	std::string          _patch_list_name;
};

std::ostream& operator<< (std::ostream&, const ChannelNameSet&);

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
	
	/// Note: channel here is 0-based while in the MIDNAM-file it's 1-based
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
	
	boost::shared_ptr<CustomDeviceMode> custom_device_mode_by_name(std::string mode_name);
	boost::shared_ptr<ChannelNameSet> channel_name_set_by_device_mode_and_channel(std::string mode, uint8_t channel);
	boost::shared_ptr<Patch> find_patch(std::string mode, uint8_t channel, PatchPrimaryKey& key);
	
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

extern const char* general_midi_program_names[128]; /* 0 .. 127 */

}

}
#endif /*MIDNAM_PATCH_H_*/
