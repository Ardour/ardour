#ifndef MIDNAM_PATCH_H_
#define MIDNAM_PATCH_H_

#include "pbd/stateful.h"
#include "midi++/event.h"
#include "pbd/xml++.h"

#include <string>
#include <list>
#include <set>

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
	typedef std::list<ChannelNameSet> ChannelNameSets;
	typedef std::list<std::string> Models;
	
	MasterDeviceNames() {};
	virtual ~MasterDeviceNames() {};
	
	const string& manufacturer() const { return _manufacturer; }
	void set_manufacturer(const string a_manufacturer) { _manufacturer = a_manufacturer; }
	
	const Models& models() const { return _models; }
	void set_models(const Models some_models) { _models = some_models; }
	
	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);
	
private:
	string _manufacturer;
	Models _models;
	ChannelNameSets _channel_name_sets;
};

class MIDINameDocument : public PBD::Stateful
{
public:
	MIDINameDocument() {};
	virtual ~MIDINameDocument() {};

	XMLNode& get_state (void);
	int      set_state (const XMLNode& a_node);

private:
	string _author;

};

}

}
#endif /*MIDNAM_PATCH_H_*/
