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
	typedef std::list<MIDI::Event> PatchMidiCommands;

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
