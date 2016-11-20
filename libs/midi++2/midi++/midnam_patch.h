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
*/

#ifndef MIDNAM_PATCH_H_
#define MIDNAM_PATCH_H_

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>

#include <stdint.h>

#include "midi++/libmidi_visibility.h"
#include "midi++/event.h"
#include "pbd/xml++.h"

namespace MIDI
{

namespace Name
{

struct LIBMIDIPP_API PatchPrimaryKey
{
public:
	PatchPrimaryKey (int program_num = 0, int bank_num = 0)
		: _bank(std::max(0, std::min(bank_num, 16383)))
		, _program(std::max(0, std::min(program_num, 127)))
	{}

	inline PatchPrimaryKey& operator=(const PatchPrimaryKey& id) {
		_bank    = id._bank;
		_program = id._program;
		return *this;
	}

	inline bool operator==(const PatchPrimaryKey& id) const {
		return (_bank    == id._bank &&
		        _program == id._program);
	}

	/** Strict weak ordering. */
	inline bool operator<(const PatchPrimaryKey& id) const {
		if (_bank < id._bank) {
			return true;
		} else if (_bank == id._bank && _program < id._program) {
			return true;
		}
		return false;
	}

	void set_bank(int bank)       { _bank    = std::max(0, std::min(bank, 16383)); }
	void set_program(int program) { _program = std::max(0, std::min(program, 127)); }

	inline uint16_t bank()    const { return _bank; }
	inline uint8_t  program() const { return _program; }

private:
	uint16_t _bank;
	uint8_t  _program;
};

class PatchBank;

class LIBMIDIPP_API Patch
{
public:

	Patch (std::string a_name = std::string(), uint8_t a_number = 0, uint16_t bank_number = 0);
	virtual ~Patch() {};

	const std::string& name() const        { return _name; }
	void set_name(const std::string& name) { _name = name; }

	const std::string& note_list_name() const  { return _note_list_name; }

	uint8_t program_number() const     { return _id.program(); }
	void set_program_number(uint8_t n) { _id.set_program(n); }

	uint16_t bank_number() const      { return _id.bank(); }
	void set_bank_number (uint16_t n) { _id.set_bank(n); }

	const PatchPrimaryKey&   patch_primary_key()   const { return _id; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string     _name;
	PatchPrimaryKey _id;
	std::string     _note_list_name;
};

typedef std::list<boost::shared_ptr<Patch> > PatchNameList;

class LIBMIDIPP_API PatchBank
{
public:
	PatchBank (uint16_t n = 0, std::string a_name = std::string()) : _name(a_name), _number (n) {};
	virtual ~PatchBank() { }

	const std::string& name() const          { return _name; }
	void set_name(const std::string& a_name) { _name = a_name; }

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

class LIBMIDIPP_API ChannelNameSet
{
public:
	typedef std::set<uint8_t>                                    AvailableForChannels;
	typedef std::list<boost::shared_ptr<PatchBank> >             PatchBanks;
	typedef std::map<PatchPrimaryKey, boost::shared_ptr<Patch> > PatchMap;
	typedef std::list<PatchPrimaryKey>                           PatchList;

	ChannelNameSet() {};
	virtual ~ChannelNameSet() {};
	ChannelNameSet(std::string& name) : _name(name) {};

	const std::string& name() const        { return _name; }
	void set_name(const std::string& name) { _name = name; }

	const PatchBanks& patch_banks() const    { return _patch_banks; }

	bool available_for_channel(uint8_t channel) const {
		return _available_for_channels.find(channel) != _available_for_channels.end();
	}

	boost::shared_ptr<Patch> find_patch(const PatchPrimaryKey& key) {
		return _patch_map[key];
	}

	boost::shared_ptr<Patch> previous_patch(const PatchPrimaryKey& key) {
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

	boost::shared_ptr<Patch> next_patch(const PatchPrimaryKey& key) {
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

	const std::string& note_list_name()    const { return _note_list_name; }
	const std::string& control_list_name() const { return _control_list_name; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

	void set_patch_banks (const PatchBanks&);
	void use_patch_name_list (const PatchNameList&);

private:
	friend std::ostream& operator<< (std::ostream&, const ChannelNameSet&);

	std::string          _name;
	AvailableForChannels _available_for_channels;
	PatchBanks           _patch_banks;
	PatchMap             _patch_map;
	PatchList            _patch_list;
	std::string          _patch_list_name;
	std::string          _note_list_name;
	std::string          _control_list_name;
};

std::ostream& operator<< (std::ostream&, const ChannelNameSet&);

class LIBMIDIPP_API Note
{
public:
	Note() {}
	Note(uint8_t number, const std::string& name) : _number(number), _name(name) {}

	const std::string& name() const        { return _name; }
	void set_name(const std::string& name) { _name = name; }

	uint8_t number() const             { return _number; }
	void    set_number(uint8_t number) { _number = number; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	uint8_t     _number;
	std::string _name;
};

class LIBMIDIPP_API NoteNameList
{
public:
	typedef std::vector< boost::shared_ptr<Note> > Notes;

	NoteNameList() { _notes.resize(128); }
	NoteNameList (const std::string& name) : _name(name) { _notes.resize(128); }

	const std::string& name() const  { return _name; }
	const Notes&       notes() const { return _notes; }

	void set_name(const std::string& name) { _name = name; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string _name;
	Notes       _notes;
};

class LIBMIDIPP_API Value
{
public:
	Value() {}
	Value(const uint16_t     number,
	      const std::string& name)
		: _number(number)
		, _name(name)
	{}

	uint16_t           number() const { return _number; }
	const std::string& name()   const { return _name; }

	void set_number(uint16_t number)       { _number = number; }
	void set_name(const std::string& name) { _name = name; }

	XMLNode& get_state(void);
	int      set_state(const XMLTree&, const XMLNode&);

private:
	uint16_t    _number;
	std::string _name;
};

class LIBMIDIPP_API ValueNameList
{
public:
	typedef std::map<uint16_t, boost::shared_ptr<Value> > Values;

	ValueNameList() {}
	ValueNameList(const std::string& name) : _name(name) {}

	const std::string& name() const { return _name; }

	void set_name(const std::string& name) { _name = name; }

	boost::shared_ptr<const Value> value(uint16_t num) const;
	boost::shared_ptr<const Value> max_value_below(uint16_t num) const;

	const Values& values() const { return _values; }

	XMLNode& get_state(void);
	int      set_state(const XMLTree&, const XMLNode&);

private:
	std::string _name;
	Values      _values;
};

class LIBMIDIPP_API Control
{
public:
	Control() {}
	Control(const std::string& type,
	        const uint16_t     number,
	        const std::string& name)
		: _type(type)
		, _number(number)
		, _name(name)
	{}

	const std::string& type()   const { return _type; }
	uint16_t           number() const { return _number; }
	const std::string& name()   const { return _name; }

	const std::string&                     value_name_list_name() const { return _value_name_list_name; }
	boost::shared_ptr<const ValueNameList> value_name_list()      const { return _value_name_list; }

	void set_type(const std::string& type) { _type = type; }
	void set_number(uint16_t number)       { _number = number; }
	void set_name(const std::string& name) { _name = name; }

	XMLNode& get_state(void);
	int      set_state(const XMLTree&, const XMLNode&);

private:
	std::string _type;
	uint16_t    _number;
	std::string _name;

	std::string                      _value_name_list_name;  ///< Global, UsesValueNameList
	boost::shared_ptr<ValueNameList> _value_name_list;       ///< Local, ValueNameList
};

class LIBMIDIPP_API ControlNameList
{
public:
	typedef std::map<uint16_t, boost::shared_ptr<Control> > Controls;

	ControlNameList() {}
	ControlNameList(const std::string& name) : _name(name) {}

	const std::string& name() const { return _name; }

	void set_name(const std::string& name) { _name = name; }

	boost::shared_ptr<const Control> control(uint16_t num) const;

	const Controls& controls() const { return _controls; }

	XMLNode& get_state(void);
	int      set_state(const XMLTree&, const XMLNode&);

private:
	std::string _name;
	Controls    _controls;
};

class LIBMIDIPP_API CustomDeviceMode
{
public:
	CustomDeviceMode() {};
	virtual ~CustomDeviceMode() {};

	const std::string& name() const        { return _name; }
	void set_name(const std::string& name) { _name = name; }


	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

	/// Note: channel here is 0-based while in the MIDNAM-file it's 1-based
	const std::string& channel_name_set_name_by_channel(uint8_t channel) {
		assert(channel <= 15);
		return _channel_name_set_assignments[channel];
	}

private:
	/// array index = channel number
	/// string contents = name of channel name set
	std::string _name;
	std::string _channel_name_set_assignments[16];
};

class LIBMIDIPP_API MasterDeviceNames
{
public:
	typedef std::set<std::string>                                       Models;
	/// maps name to CustomDeviceMode
	typedef std::map<std::string, boost::shared_ptr<CustomDeviceMode> > CustomDeviceModes;
	typedef std::list<std::string>                                      CustomDeviceModeNames;
	/// maps name to ChannelNameSet
	typedef std::map<std::string, boost::shared_ptr<ChannelNameSet> >   ChannelNameSets;
	typedef std::map<std::string, boost::shared_ptr<NoteNameList> >     NoteNameLists;
	typedef std::map<std::string, boost::shared_ptr<ControlNameList> >  ControlNameLists;
	typedef std::map<std::string, boost::shared_ptr<ValueNameList> >    ValueNameLists;
	typedef std::map<std::string, PatchNameList>                        PatchNameLists;

	MasterDeviceNames() {};
	virtual ~MasterDeviceNames() {};

	const std::string& manufacturer() const { return _manufacturer; }
	void set_manufacturer(const std::string& manufacturer) { _manufacturer = manufacturer; }

	const Models& models() const { return _models; }
	void set_models(const Models some_models) { _models = some_models; }

	const ControlNameLists& controls() const { return _control_name_lists; }
	const ValueNameLists&   values()   const { return _value_name_lists; }

	boost::shared_ptr<const ValueNameList> value_name_list_by_control(
		const std::string& mode,
		uint8_t            channel,
		uint8_t            number);

	const CustomDeviceModeNames& custom_device_mode_names() const { return _custom_device_mode_names; }

	boost::shared_ptr<CustomDeviceMode> custom_device_mode_by_name(const std::string& mode_name);
	boost::shared_ptr<ChannelNameSet> channel_name_set_by_channel(const std::string& mode, uint8_t channel);
	boost::shared_ptr<Patch> find_patch(const std::string& mode, uint8_t channel, const PatchPrimaryKey& key);

	boost::shared_ptr<ControlNameList> control_name_list(const std::string& name);
	boost::shared_ptr<ValueNameList>   value_name_list(const std::string& name);
	boost::shared_ptr<NoteNameList>    note_name_list(const std::string& name);
	boost::shared_ptr<ChannelNameSet>  channel_name_set(const std::string& name);

	std::string note_name(const std::string& mode_name,
	                      uint8_t            channel,
	                      uint16_t           bank,
	                      uint8_t            program,
	                      uint8_t            number);

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
	ControlNameLists      _control_name_lists;
	ValueNameLists        _value_name_lists;
};

class LIBMIDIPP_API MIDINameDocument
{
public:
	// Maps Model names to MasterDeviceNames
	typedef std::map<std::string, boost::shared_ptr<MasterDeviceNames> > MasterDeviceNamesList;

	MIDINameDocument() {}
	MIDINameDocument(const std::string& file_path);
	virtual ~MIDINameDocument() {};

	const std::string& file_path () const { return _file_path; }
	const std::string& author() const { return _author; }

	void set_author(const std::string& author) { _author = author; }
	void set_file_path(const std::string& file_path) { _file_path = file_path; }

	boost::shared_ptr<MasterDeviceNames> master_device_names(const std::string& model);

	const MasterDeviceNamesList& master_device_names_by_model() const { return _master_device_names_list; }

	const MasterDeviceNames::Models& all_models() const { return _all_models; }

	XMLNode& get_state (void);
	int      set_state (const XMLTree&, const XMLNode&);

private:
	std::string                   _file_path;
	std::string                   _author;
	MasterDeviceNamesList         _master_device_names_list;
	MasterDeviceNames::Models     _all_models;
};

LIBMIDIPP_API extern const char* general_midi_program_names[128]; /* 0 .. 127 */

}

}
#endif /*MIDNAM_PATCH_H_*/
