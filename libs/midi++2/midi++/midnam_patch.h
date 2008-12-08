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

#include <string>
#include <list>
#include <set>

#include "pbd/stateful.h"
#include "midi++/event.h"
#include "pbd/xml++.h"

namespace MIDI
{

namespace Name
{

class Patch : public PBD::Stateful
{
public:
	typedef std::list<Evoral::Event> PatchMidiCommands;

	Patch() {};
	Patch(string a_number, string a_name) : _number(a_number), _name(a_name) {};
	~Patch() {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	const string& number() const             { return _number; }
	void set_number(const string a_number)   { _number = a_number; }

	const PatchMidiCommands& patch_midi_commands() const { return _patch_midi_commands; }

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string _number;
	string _name;
	PatchMidiCommands _patch_midi_commands;
};

class PatchBank : public PBD::Stateful
{
public:
	typedef std::list<Patch> PatchNameList;

	PatchBank() {};
	virtual ~PatchBank() {};
	PatchBank(string a_name) : _name(a_name) {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	const PatchNameList& patch_name_list() const { return _patch_name_list; }

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string        _name;
	PatchNameList _patch_name_list;
};

class ChannelNameSet : public PBD::Stateful
{
public:
	typedef std::set<uint8_t>    AvailableForChannels;
	typedef std::list<PatchBank> PatchBanks;

	ChannelNameSet() {};
	virtual ~ChannelNameSet() {};
	ChannelNameSet(string a_name) : _name(a_name) {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	const AvailableForChannels& available_for_channels() const { return _available_for_channels; }
	const PatchBanks&           patch_banks()            const { return _patch_banks; }

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string _name;
	AvailableForChannels _available_for_channels;
	PatchBanks           _patch_banks;
};

class Note : public PBD::Stateful
{
public:
	Note() {};
	Note(string a_number, string a_name) : _number(a_number), _name(a_name) {};
	~Note() {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	const string& number() const             { return _number; }
	void set_number(const string a_number)   { _number = a_number; }

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string _number;
	string _name;
};

class NoteNameList : public PBD::Stateful
{
public:
	typedef std::list<Note> Notes;
	NoteNameList() {};
	NoteNameList(string a_name) : _name(a_name) {};
	~NoteNameList() {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	const Notes& notes() const { return _notes; }

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string _name;
	Notes  _notes;
};

class CustomDeviceMode : public PBD::Stateful
{
public:
	CustomDeviceMode() {};
	virtual ~CustomDeviceMode() {};

	const string& name() const               { return _name; }
	void set_name(const string a_name)       { _name = a_name; }

	
	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);
	
private:
	/// array index = channel number
	/// string contents = name of channel name set 
	string _name;
	string _channel_name_set_assignments[16];
};

class MasterDeviceNames : public PBD::Stateful
{
public:
	typedef std::list<std::string>       Models;
	typedef std::list<CustomDeviceMode>  CustomDeviceModes;
	typedef std::list<ChannelNameSet>    ChannelNameSets;
	typedef std::list<NoteNameList>      NoteNameLists;
	
	
	MasterDeviceNames() {};
	virtual ~MasterDeviceNames() {};
	
	const string& manufacturer() const { return _manufacturer; }
	void set_manufacturer(const string a_manufacturer) { _manufacturer = a_manufacturer; }
	
	const Models& models() const { return _models; }
	void set_models(const Models some_models) { _models = some_models; }
	
	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);
	
private:
	string            _manufacturer;
	Models            _models;
	CustomDeviceModes _custom_device_modes;
	ChannelNameSets   _channel_name_sets;
	NoteNameLists     _note_name_lists;
};

class MIDINameDocument : public PBD::Stateful
{
public:
	typedef std::list<MasterDeviceNames> MasterDeviceNamesList;
	
	MIDINameDocument() {};
	MIDINameDocument(const string &filename) : _document(XMLTree(filename)) { set_state(*_document.root()); };
	virtual ~MIDINameDocument() {};

	const string& author() const { return _author; }
	void set_author(const string an_author) { _author = an_author; }
	
	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string                _author;
	MasterDeviceNamesList _master_device_names_list;
	XMLTree               _document;
};

}

}
#endif /*MIDNAM_PATCH_H_*/
