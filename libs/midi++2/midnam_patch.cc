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
*/

#include <stdlib.h>

#include <algorithm>
#include <iostream>

#include "midi++/midnam_patch.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

using namespace std;
using PBD::error;

namespace MIDI
{

namespace Name
{
	
Patch::Patch (std::string name, uint8_t p_number, uint16_t b_number)
	: _name (name)
	, _id (p_number, b_number)
{
}

static int
string_to_int(const XMLTree& tree, const std::string& str)
{
	char*     endptr = NULL;
	const int i      = strtol(str.c_str(), &endptr, 10);
	if (str.empty() || *endptr != '\0') {
		PBD::error << string_compose("%1: Bad number `%2'", tree.filename(), str)
		           << endmsg;
	}
	return i;
}

static int
initialize_primary_key_from_commands (
	const XMLTree& tree, PatchPrimaryKey& id, const XMLNode* node)
{
	id.bank_number = 0;

	const XMLNodeList events = node->children();
	for (XMLNodeList::const_iterator i = events.begin(); i != events.end(); ++i) {

		XMLNode* node = *i;
		if (node->name() == "ControlChange") {
			const string& control = node->property("Control")->value();
			const string& value   = node->property("Value")->value();

			if (control == "0") {
				id.bank_number |= string_to_int(tree, value) << 7;
			} else if (control == "32") {
				id.bank_number |= string_to_int(tree, value);
			}

		} else if (node->name() == "ProgramChange") {
			const string& number = node->property("Number")->value();
			assert(number != "");
			id.program_number = string_to_int(tree, number);
		}
	}

	return 0;
}

XMLNode&
Patch::get_state (void)
{
	XMLNode* node = new XMLNode("Patch");

	/* XXX this is totally wrong */

	node->add_property("Number", string_compose ("%1", _id.program_number));
	node->add_property("Name",   _name);

	/*
	typedef std::list< boost::shared_ptr< Evoral::MIDIEvent<double> > > PatchMidiCommands;
	XMLNode* commands = node->add_child("PatchMIDICommands");
	for (PatchMidiCommands::const_iterator event = _patch_midi_commands.begin();
	    event != _patch_midi_commands.end();
	    ++event) {
		commands->add_child_copy(*((((Evoral::MIDIEvent&)*event)).to_xml()));
	}
	*/

	return *node;
}

int
Patch::set_state (const XMLTree& tree, const XMLNode& node)
{
	if (node.name() != "Patch") {
		cerr << "Incorrect node " << node.name() << " handed to Patch" << endl;
		return -1;
	}

	/* Note there is a "Number" attribute, but it's really more like a label
	   and is often not numeric.  We currently do not use it. */

	const XMLProperty* program_change = node.property("ProgramChange");
	if (program_change) {
		_id.program_number = string_to_int(tree, program_change->value());
	}

	const XMLProperty* name = node.property("Name");
	if (!name) {
		return -1;
	}
	_name = name->value();

	XMLNode* commands = node.child("PatchMIDICommands");
	if (commands) {
		if (initialize_primary_key_from_commands(tree, _id, commands) &&
		    !program_change) {
			return -1;  // Failed to find a program number anywhere
		}
	}
	
	XMLNode* use_note_name_list = node.child("UsesNoteNameList");
	if (use_note_name_list) {
		_note_list_name = use_note_name_list->property ("Name")->value();
	}

	return 0;
}

XMLNode&
Note::get_state (void)
{
	XMLNode* node = new XMLNode("Note");
	node->add_property("Number", _number + 1);
	node->add_property("Name",   _name);

	return *node;
}

int
Note::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "Note");

	const int num = string_to_int(tree, node.property("Number")->value());
	if (num < 1 || num > 128) {
		PBD::warning << string_compose("%1: Note number %2 (%3) out of range",
		                               tree.filename(), num, _name)
		             << endmsg;
		return -1;
	}

	_number = num - 1;
	_name   = node.property("Name")->value();

	return 0;
}

XMLNode&
NoteNameList::get_state (void)
{
	XMLNode* node = new XMLNode("NoteNameList");
	node->add_property("Name", _name);

	return *node;
}

static void
add_note_from_xml (NoteNameList::Notes& notes, const XMLTree& tree, const XMLNode& node)
{
	boost::shared_ptr<Note> note(new Note());
	if (!note->set_state (tree, node)) {
		if (!notes[note->number()]) {
			notes[note->number()] = note;
		} else {
			PBD::warning
				<< string_compose("%1: Duplicate note number %2 (%3) ignored",
				                  tree.filename(), (int)note->number(), note->name())
				<< endmsg;
		}
	}
}

int
NoteNameList::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "NoteNameList");
	_name = node.property("Name")->value();
	_notes.clear();
	_notes.resize(128);

	for (XMLNodeList::const_iterator i = node.children().begin();
	     i != node.children().end(); ++i) {
		if ((*i)->name() == "Note") {
			add_note_from_xml(_notes, tree, **i);
		} else if ((*i)->name() == "NoteGroup") {
			for (XMLNodeList::const_iterator j = (*i)->children().begin();
			     j != (*i)->children().end(); ++j) {
				if ((*j)->name() == "Note") {
					add_note_from_xml(_notes, tree, **j);
				} else {
					PBD::warning << string_compose("%1: Invalid NoteGroup child %2 ignored",
					                               tree.filename(), (*j)->name())
					             << endmsg;
				}
			}
		}
	}

	return 0;
}

XMLNode&
Control::get_state (void)
{
	XMLNode* node = new XMLNode("Control");
	node->add_property("Type",   _type);
	node->add_property("Number", _number);
	node->add_property("Name",   _name);

	return *node;
}

int
Control::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "Control");
	if (node.property("Type")) {
		_type = node.property("Type")->value();
	} else {
		_type = "7bit";
	}
	_number = string_to_int(tree, node.property("Number")->value());
	_name   = node.property("Name")->value();

	for (XMLNodeList::const_iterator i = node.children().begin();
	     i != node.children().end(); ++i) {
		if ((*i)->name() == "Values") {
			// <Values> has Min and Max properties, but we don't care
			for (XMLNodeList::const_iterator j = (*i)->children().begin();
			     j != (*i)->children().end(); ++j) {
				if ((*j)->name() == "ValueNameList") {
					_value_name_list = boost::shared_ptr<ValueNameList>(new ValueNameList());
					_value_name_list->set_state(tree, **j);
				} else if ((*j)->name() == "UsesValueNameList") {
					_value_name_list_name = (*j)->property("Name")->value();
				}
			}
		}
	}

	return 0;
}

XMLNode&
ControlNameList::get_state (void)
{
	XMLNode* node = new XMLNode("ControlNameList");
	node->add_property("Name", _name);

	return *node;
}

int
ControlNameList::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "ControlNameList");
	_name = node.property("Name")->value();

	_controls.clear();
	for (XMLNodeList::const_iterator i = node.children().begin();
	     i != node.children().end(); ++i) {
		if ((*i)->name() == "Control") {
			boost::shared_ptr<Control> control(new Control());
			control->set_state (tree, *(*i));
			if (_controls.find(control->number()) == _controls.end()) {
				_controls.insert(make_pair(control->number(), control));
			} else {
				PBD::warning << string_compose("%1: Duplicate control %2 ignored",
				                               tree.filename(), control->number())
				             << endmsg;
			}
		}
	}

	return 0;
}

boost::shared_ptr<const Control>
ControlNameList::control(uint16_t num) const
{
	Controls::const_iterator i = _controls.find(num);
	if (i != _controls.end()) {
		return i->second;
	}
	return boost::shared_ptr<const Control>();
}

XMLNode&
Value::get_state (void)
{
	XMLNode* node = new XMLNode("Value");
	node->add_property("Number", _number);
	node->add_property("Name",   _name);

	return *node;
}

int
Value::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "Value");
	_number = string_to_int(tree, node.property("Number")->value());
	_name   = node.property("Name")->value();

	return 0;
}

XMLNode&
ValueNameList::get_state (void)
{
	XMLNode* node = new XMLNode("ValueNameList");
	node->add_property("Name", _name);

	return *node;
}

int
ValueNameList::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "ValueNameList");
	const XMLProperty* name_prop = node.property("Name");
	if (name_prop) {
		// May be anonymous if written inline within a single <Control> tag
		_name = name_prop->value();
	}

	_values.clear();
	for (XMLNodeList::const_iterator i = node.children().begin();
	     i != node.children().end(); ++i) {
		if ((*i)->name() == "Value") {
			boost::shared_ptr<Value> value(new Value());
			value->set_state (tree, *(*i));
			if (_values.find(value->number()) == _values.end()) {
				_values.insert(make_pair(value->number(), value));
			} else {
				PBD::warning << string_compose("%1: Duplicate value %2 ignored",
				                               tree.filename(), value->number())
				             << endmsg;
			}
		}
	}

	return 0;
}

boost::shared_ptr<const Value>
ValueNameList::value(uint16_t num) const
{
	Values::const_iterator i = _values.find(num);
	if (i != _values.end()) {
		return i->second;
	}
	return boost::shared_ptr<const Value>();
}

boost::shared_ptr<const Value>
ValueNameList::max_value_below(uint16_t num) const
{
	Values::const_iterator i = _values.lower_bound(num);
	if (i->first == num) {
		// Exact match
		return i->second;
	} else if (i == _values.begin()) {
		// No value is < num
		return boost::shared_ptr<const Value>();
	} else {
		// Found the smallest element >= num, so the previous one is our result
		--i;
		return i->second;
	}
}

XMLNode&
PatchBank::get_state (void)
{
	XMLNode* node = new XMLNode("PatchBank");
	node->add_property("Name",   _name);
	XMLNode* patch_name_list = node->add_child("PatchNameList");
	for (PatchNameList::iterator patch = _patch_name_list.begin();
	    patch != _patch_name_list.end();
	    ++patch) {
		patch_name_list->add_child_nocopy((*patch)->get_state());
	}

	return *node;
}

int
PatchBank::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "PatchBank");
	_name   = node.property("Name")->value();

	XMLNode* commands = node.child("MIDICommands");
	if (commands) {
		PatchPrimaryKey id (0, 0);
		if (initialize_primary_key_from_commands (tree, id, commands)) {
			return -1;
		}
		_number = id.bank_number;
	}

	XMLNode* patch_name_list = node.child("PatchNameList");

	if (patch_name_list) {
		const XMLNodeList patches = patch_name_list->children();
		for (XMLNodeList::const_iterator i = patches.begin(); i != patches.end(); ++i) {
			boost::shared_ptr<Patch> patch (new Patch (string(), 0, _number));
			patch->set_state(tree, *(*i));
			_patch_name_list.push_back(patch);
		}
	} else {
		XMLNode* use_patch_name_list = node.child ("UsesPatchNameList");
		if (use_patch_name_list) {
			_patch_list_name = use_patch_name_list->property ("Name")->value();
		} else {
			error << "Patch without patch name list - patchfile will be ignored" << endmsg;
			return -1;
		}
	}

	return 0;
}

int
PatchBank::set_patch_name_list (const PatchNameList& pnl)
{
	_patch_name_list = pnl;
	_patch_list_name = "";

	for (PatchNameList::iterator p = _patch_name_list.begin(); p != _patch_name_list.end(); p++) {
		(*p)->set_bank_number (_number);
	}

	return 0;
}

std::ostream&
operator<< (std::ostream& os, const ChannelNameSet& cns)
{
	os << "Channel Name Set: name = " << cns._name << endl
	   << "Map size " << cns._patch_map.size () << endl
	   << "List size " << cns._patch_list.size() << endl
	   << "Patch list name = [" << cns._patch_list_name << ']' << endl
	   << "Available channels : ";
	for (set<uint8_t>::iterator x = cns._available_for_channels.begin(); x != cns._available_for_channels.end(); ++x) {
		os << (int) (*x) << ' ';
	}
	os << endl;
	
	for (ChannelNameSet::PatchBanks::const_iterator pbi = cns._patch_banks.begin(); pbi != cns._patch_banks.end(); ++pbi) {
		os << "\tPatch Bank " << (*pbi)->name() << " with " << (*pbi)->patch_name_list().size() << " patches\n";
		for (PatchNameList::const_iterator pni = (*pbi)->patch_name_list().begin(); pni != (*pbi)->patch_name_list().end(); ++pni) {
			os << "\t\tPatch name " << (*pni)->name() << " prog " << (int) (*pni)->program_number() << " bank " << (*pni)->bank_number() << endl;
		}
	}

	return os;
}

void
ChannelNameSet::set_patch_banks (const ChannelNameSet::PatchBanks& pb)
{
	_patch_banks = pb;
	
	_patch_map.clear ();
	_patch_list.clear ();
	_patch_list_name = "";
	_available_for_channels.clear ();
	
	for (PatchBanks::const_iterator pbi = _patch_banks.begin(); pbi != _patch_banks.end(); ++pbi) {
		for (PatchNameList::const_iterator pni = (*pbi)->patch_name_list().begin(); pni != (*pbi)->patch_name_list().end(); ++pni) {
			_patch_map[(*pni)->patch_primary_key()] = (*pni);
			_patch_list.push_back ((*pni)->patch_primary_key());
		}
	}

	for (uint8_t n = 0; n < 16; ++n) {
		_available_for_channels.insert (n);
	}
}

void
ChannelNameSet::use_patch_name_list (const PatchNameList& pnl)
{
	for (PatchNameList::const_iterator p = pnl.begin(); p != pnl.end(); ++p) {
		_patch_map[(*p)->patch_primary_key()] = (*p);
		_patch_list.push_back ((*p)->patch_primary_key());
	}
}

XMLNode&
ChannelNameSet::get_state (void)
{
	XMLNode* node = new XMLNode("ChannelNameSet");
	node->add_property("Name",   _name);

	XMLNode* available_for_channels = node->add_child("AvailableForChannels");
	assert(available_for_channels);

	for (uint8_t channel = 0; channel < 16; ++channel) {
		XMLNode* available_channel = available_for_channels->add_child("AvailableChannel");
		assert(available_channel);

		available_channel->add_property("Channel", (long) channel);

		if (_available_for_channels.find(channel) != _available_for_channels.end()) {
			available_channel->add_property("Available", "true");
		} else {
			available_channel->add_property("Available", "false");
		}
	}

	for (PatchBanks::iterator patch_bank = _patch_banks.begin();
	    patch_bank != _patch_banks.end();
	    ++patch_bank) {
		node->add_child_nocopy((*patch_bank)->get_state());
	}

	return *node;
}

int
ChannelNameSet::set_state (const XMLTree& tree, const XMLNode& node)
{
	assert(node.name() == "ChannelNameSet");
	_name = node.property("Name")->value();

	const XMLNodeList children = node.children();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		XMLNode* node = *i;
		assert(node);
		if (node->name() == "AvailableForChannels") {
			boost::shared_ptr<XMLSharedNodeList> channels =
				tree.find("//AvailableChannel[@Available = 'true']/@Channel", node);
			for (XMLSharedNodeList::const_iterator i = channels->begin();
			    i != channels->end();
			    ++i) {
				_available_for_channels.insert(
					string_to_int(tree, (*i)->attribute_value()));
			}
		} else if (node->name() == "PatchBank") {
			boost::shared_ptr<PatchBank> bank (new PatchBank ());
			bank->set_state(tree, *node);
			_patch_banks.push_back(bank);
			const PatchNameList& patches = bank->patch_name_list();
			for (PatchNameList::const_iterator patch = patches.begin();
			     patch != patches.end();
			     ++patch) {
				_patch_map[(*patch)->patch_primary_key()] = *patch;
				_patch_list.push_back((*patch)->patch_primary_key());
			}
		} else if (node->name() == "UsesNoteNameList") {
			_note_list_name = node->property ("Name")->value();
		} else if (node->name() == "UsesControlNameList") {
			_control_list_name = node->property ("Name")->value();
		}
	}

	return 0;
}

int
CustomDeviceMode::set_state(const XMLTree& tree, const XMLNode& a_node)
{
	assert(a_node.name() == "CustomDeviceMode");

	_name = a_node.property("Name")->value();

	boost::shared_ptr<XMLSharedNodeList> channel_name_set_assignments =
		tree.find("//ChannelNameSetAssign", const_cast<XMLNode *>(&a_node));
	for (XMLSharedNodeList::const_iterator i = channel_name_set_assignments->begin();
	    i != channel_name_set_assignments->end();
	    ++i) {
		const int     channel  = string_to_int(tree, (*i)->property("Channel")->value());
		const string& name_set = (*i)->property("NameSet")->value();
		assert( 1 <= channel && channel <= 16 );
		_channel_name_set_assignments[channel - 1] = name_set;
	}
	return 0;
}

XMLNode&
CustomDeviceMode::get_state(void)
{
	XMLNode* custom_device_mode = new XMLNode("CustomDeviceMode");
	custom_device_mode->add_property("Name",   _name);
	XMLNode* channel_name_set_assignments =
		custom_device_mode->add_child("ChannelNameSetAssignments");
	for (int i = 0; i < 15 && !_channel_name_set_assignments[i].empty(); i++) {
		XMLNode* channel_name_set_assign =
			channel_name_set_assignments->add_child("ChannelNameSetAssign");
		channel_name_set_assign->add_property("Channel", i + 1);
		channel_name_set_assign->add_property("NameSet", _channel_name_set_assignments[i]);
	}

	return *custom_device_mode;
}

boost::shared_ptr<const ValueNameList>
MasterDeviceNames::value_name_list_by_control(const std::string& mode, uint8_t channel, uint8_t number)
{
	boost::shared_ptr<ChannelNameSet> chan_names = channel_name_set_by_channel(mode, channel);
	if (!chan_names) {
		return boost::shared_ptr<const ValueNameList>();
	}

	boost::shared_ptr<ControlNameList> control_names = control_name_list(chan_names->control_list_name());
	if (!control_names) {
		return boost::shared_ptr<const ValueNameList>();
	}

	boost::shared_ptr<const Control> control = control_names->control(number);
	if (!control) {
		return boost::shared_ptr<const ValueNameList>();
	}

	if (!control->value_name_list_name().empty()) {
		return value_name_list(control->value_name_list_name());
	} else {
		return control->value_name_list();
	}
}

boost::shared_ptr<CustomDeviceMode> 
MasterDeviceNames::custom_device_mode_by_name(const std::string& mode_name)
{
	return _custom_device_modes[mode_name];
}

boost::shared_ptr<ChannelNameSet> 
MasterDeviceNames::channel_name_set_by_channel(const std::string& mode, uint8_t channel)
{
	boost::shared_ptr<CustomDeviceMode> cdm = custom_device_mode_by_name(mode);
	boost::shared_ptr<ChannelNameSet> cns =  _channel_name_sets[cdm->channel_name_set_name_by_channel(channel)];
	return cns;
}

boost::shared_ptr<Patch> 
MasterDeviceNames::find_patch(const std::string& mode, uint8_t channel, const PatchPrimaryKey& key) 
{
	boost::shared_ptr<ChannelNameSet> cns = channel_name_set_by_channel(mode, channel);
	if (!cns) return boost::shared_ptr<Patch>();
	return cns->find_patch(key);
}

boost::shared_ptr<ChannelNameSet>
MasterDeviceNames::channel_name_set(const std::string& name)
{
	ChannelNameSets::const_iterator i = _channel_name_sets.find(name);
	if (i != _channel_name_sets.end()) {
		return i->second;
	}
	return boost::shared_ptr<ChannelNameSet>();
}

boost::shared_ptr<ControlNameList>
MasterDeviceNames::control_name_list(const std::string& name)
{
	ControlNameLists::const_iterator i = _control_name_lists.find(name);
	if (i != _control_name_lists.end()) {
		return i->second;
	}
	return boost::shared_ptr<ControlNameList>();
}

boost::shared_ptr<ValueNameList>
MasterDeviceNames::value_name_list(const std::string& name)
{
	ValueNameLists::const_iterator i = _value_name_lists.find(name);
	if (i != _value_name_lists.end()) {
		return i->second;
	}
	return boost::shared_ptr<ValueNameList>();
}

boost::shared_ptr<NoteNameList>
MasterDeviceNames::note_name_list(const std::string& name)
{
	NoteNameLists::const_iterator i = _note_name_lists.find(name);
	if (i != _note_name_lists.end()) {
		return i->second;
	}
	return boost::shared_ptr<NoteNameList>();
}

std::string
MasterDeviceNames::note_name(const std::string& mode_name,
                             uint8_t            channel,
                             uint16_t           bank,
                             uint8_t            program,
                             uint8_t            number)
{
	if (number > 127) {
		return "";
	}

	boost::shared_ptr<const Patch> patch(
		find_patch(mode_name, channel, PatchPrimaryKey(program, bank)));
	if (!patch) {
		return "";
	}

	boost::shared_ptr<const NoteNameList> note_names(
		note_name_list(patch->note_list_name()));
	if (!note_names) {
		/* No note names specific to this patch, check the ChannelNameSet */
		boost::shared_ptr<ChannelNameSet> chan_names = channel_name_set_by_channel(
			mode_name, channel);
		if (chan_names) {
			note_names = note_name_list(chan_names->note_list_name());
		}
	}
	if (!note_names) {
		return "";
	}

	boost::shared_ptr<const Note> note(note_names->notes()[number]);
	return note ? note->name() : "";
}

int
MasterDeviceNames::set_state(const XMLTree& tree, const XMLNode&)
{
	// Manufacturer
	boost::shared_ptr<XMLSharedNodeList> manufacturer = tree.find("//Manufacturer");
	assert(manufacturer->size() == 1);
	_manufacturer = manufacturer->front()->children().front()->content();

	// Models
	boost::shared_ptr<XMLSharedNodeList> models = tree.find("//Model");
	assert(models->size() >= 1);
	for (XMLSharedNodeList::iterator i = models->begin();
	     i != models->end();
	     ++i) {
		const XMLNodeList& contents = (*i)->children();
		assert(contents.size() == 1);
		XMLNode * content = *(contents.begin());
		assert(content->is_content());
		_models.insert(content->content());
	}

	// CustomDeviceModes
	boost::shared_ptr<XMLSharedNodeList> custom_device_modes = tree.find("//CustomDeviceMode");
	for (XMLSharedNodeList::iterator i = custom_device_modes->begin();
	     i != custom_device_modes->end();
	     ++i) {
		boost::shared_ptr<CustomDeviceMode> custom_device_mode(new CustomDeviceMode());
		custom_device_mode->set_state(tree, *(*i));

		_custom_device_modes[custom_device_mode->name()] = custom_device_mode;
		_custom_device_mode_names.push_back(custom_device_mode->name());
	}

	// ChannelNameSets
	boost::shared_ptr<XMLSharedNodeList> channel_name_sets = tree.find("//ChannelNameSet");
	for (XMLSharedNodeList::iterator i = channel_name_sets->begin();
	     i != channel_name_sets->end();
	     ++i) {
		boost::shared_ptr<ChannelNameSet> channel_name_set(new ChannelNameSet());
		channel_name_set->set_state(tree, *(*i));
		_channel_name_sets[channel_name_set->name()] = channel_name_set;
	}

	// NoteNameLists
	boost::shared_ptr<XMLSharedNodeList> note_name_lists = tree.find("//NoteNameList");
	for (XMLSharedNodeList::iterator i = note_name_lists->begin();
	     i != note_name_lists->end();
	     ++i) {
		boost::shared_ptr<NoteNameList> note_name_list(new NoteNameList());
		note_name_list->set_state (tree, *(*i));
		_note_name_lists[note_name_list->name()] = note_name_list;
	}

	// ControlNameLists
	boost::shared_ptr<XMLSharedNodeList> control_name_lists = tree.find("//ControlNameList");
	for (XMLSharedNodeList::iterator i = control_name_lists->begin();
	     i != control_name_lists->end();
	     ++i) {
		boost::shared_ptr<ControlNameList> control_name_list(new ControlNameList());
		control_name_list->set_state (tree, *(*i));
		_control_name_lists[control_name_list->name()] = control_name_list;
	}

	// ValueNameLists
	boost::shared_ptr<XMLSharedNodeList> value_name_lists = tree.find("/child::MIDINameDocument/child::MasterDeviceNames/child::ValueNameList");
	for (XMLSharedNodeList::iterator i = value_name_lists->begin();
	     i != value_name_lists->end();
	     ++i) {
		boost::shared_ptr<ValueNameList> value_name_list(new ValueNameList());
		value_name_list->set_state (tree, *(*i));
		_value_name_lists[value_name_list->name()] = value_name_list;
	}

	// global/post-facto PatchNameLists
	boost::shared_ptr<XMLSharedNodeList> patch_name_lists = tree.find("/child::MIDINameDocument/child::MasterDeviceNames/child::PatchNameList");
	for (XMLSharedNodeList::iterator i = patch_name_lists->begin();
	     i != patch_name_lists->end();
	     ++i) {

		PatchNameList patch_name_list;
		const XMLNodeList patches = (*i)->children();

		for (XMLNodeList::const_iterator p = patches.begin(); p != patches.end(); ++p) {
			boost::shared_ptr<Patch> patch (new Patch ());
			patch->set_state(tree, *(*p));
			patch_name_list.push_back(patch);
		}

		if (!patch_name_list.empty()) {
			_patch_name_lists[(*i)->property ("Name")->value()] = patch_name_list;
		}
	}

	/* now traverse patches and hook up anything that used UsePatchNameList
	 * to the right patch list
	 */

	for (ChannelNameSets::iterator cns = _channel_name_sets.begin(); cns != _channel_name_sets.end(); ++cns) {
		ChannelNameSet::PatchBanks pbs = cns->second->patch_banks();
		PatchNameLists::iterator p;

		for (ChannelNameSet::PatchBanks::iterator pb = pbs.begin(); pb != pbs.end(); ++pb) {
			const std::string& pln = (*pb)->patch_list_name();
			if (!pln.empty()) {
				if ((p = _patch_name_lists.find (pln)) != _patch_name_lists.end()) {
					if ((*pb)->set_patch_name_list (p->second)) {
						return -1;
					}
					cns->second->use_patch_name_list (p->second);
				} else {
					error << string_compose ("Patch list name %1 was not found - patch file ignored", pln) << endmsg;
					return -1;
				}
			}
		}

	}

	return 0;
}

XMLNode&
MasterDeviceNames::get_state(void)
{
	static XMLNode nothing("<nothing>");
	return nothing;
}

MIDINameDocument::MIDINameDocument (const string& filename)
{
	if (!_document.read (filename)) {
		throw failed_constructor ();
	}

	_document.set_filename (filename);
	set_state (_document, *_document.root());
}

int
MIDINameDocument::set_state (const XMLTree& tree, const XMLNode&)
{
	// Author

	boost::shared_ptr<XMLSharedNodeList> author = tree.find("//Author");
	if (author->size() < 1) {
		error << "No author information in MIDNAM file" << endmsg;
		return -1;
	}
	
	if (author->front()->children().size() > 0) {
		_author = author->front()->children().front()->content();
	}

	// MasterDeviceNames

	boost::shared_ptr<XMLSharedNodeList> master_device_names_list = tree.find ("//MasterDeviceNames");

	for (XMLSharedNodeList::iterator i = master_device_names_list->begin();
	     i != master_device_names_list->end();
	     ++i) {
		boost::shared_ptr<MasterDeviceNames> master_device_names(new MasterDeviceNames());

		if (master_device_names->set_state(tree, *(*i))) {
			return -1;
		}

		for (MasterDeviceNames::Models::const_iterator model = master_device_names->models().begin();
		     model != master_device_names->models().end();
		     ++model) {
			_master_device_names_list.insert(
				std::pair<std::string, boost::shared_ptr<MasterDeviceNames> >
				(*model,      master_device_names));
			
			_all_models.insert(*model);
		}
	}

	return 0;
}

XMLNode&
MIDINameDocument::get_state(void)
{
	static XMLNode nothing("<nothing>");
	return nothing;
}

boost::shared_ptr<MasterDeviceNames>
MIDINameDocument::master_device_names(const std::string& model)
{
	MasterDeviceNamesList::const_iterator m = _master_device_names_list.find(model);
	if (m != _master_device_names_list.end()) {
		return boost::shared_ptr<MasterDeviceNames>(m->second);
	}
	return boost::shared_ptr<MasterDeviceNames>();
}

const char* general_midi_program_names[128] = {
	"Acoustic Grand Piano",
	"Bright Acoustic Piano",
	"Electric Grand Piano",
	"Honky-tonk Piano",
	"Rhodes Piano",
	"Chorused Piano",
	"Harpsichord",
	"Clavinet",
	"Celesta",
	"Glockenspiel",
	"Music Box",
	"Vibraphone",
	"Marimba",
	"Xylophone",
	"Tubular Bells",
	"Dulcimer",
	"Hammond Organ",
	"Percussive Organ",
	"Rock Organ",
	"Church Organ",
	"Reed Organ",
	"Accordion",
	"Harmonica",
	"Tango Accordion",
	"Acoustic Guitar (nylon)",
	"Acoustic Guitar (steel)",
	"Electric Guitar (jazz)",
	"Electric Guitar (clean)",
	"Electric Guitar (muted)",
	"Overdriven Guitar",
	"Distortion Guitar",
	"Guitar Harmonics",
	"Acoustic Bass",
	"Electric Bass (finger)",
	"Electric Bass (pick)",
	"Fretless Bass",
	"Slap Bass 1",
	"Slap Bass 2",
	"Synth Bass 1",
	"Synth Bass 2",
	"Violin",
	"Viola",
	"Cello",
	"Contrabass",
	"Tremolo Strings",
	"Pizzicato Strings",
	"Orchestral Harp",
	"Timpani",
	"String Ensemble 1",
	"String Ensemble 2",
	"SynthStrings 1",
	"SynthStrings 2",
	"Choir Aahs",
	"Voice Oohs",
	"Synth Voice",
	"Orchestra Hit",
	"Trumpet",
	"Trombone",
	"Tuba",
	"Muted Trumpet",
	"French Horn",
	"Brass Section",
	"Synth Brass 1",
	"Synth Brass 2",
	"Soprano Sax",
	"Alto Sax",
	"Tenor Sax",
	"Baritone Sax",
	"Oboe",
	"English Horn",
	"Bassoon",
	"Clarinet",
	"Piccolo",
	"Flute",
	"Recorder",
	"Pan Flute",
	"Bottle Blow",
	"Shakuhachi",
	"Whistle",
	"Ocarina",
	"Lead 1 (square)",
	"Lead 2 (sawtooth)",
	"Lead 3 (calliope lead)",
	"Lead 4 (chiff lead)",
	"Lead 5 (charang)",
	"Lead 6 (voice)",
	"Lead 7 (fifths)",
	"Lead 8 (bass + lead)",
	"Pad 1 (new age)",
	"Pad 2 (warm)",
	"Pad 3 (polysynth)",
	"Pad 4 (choir)",
	"Pad 5 (bowed)",
	"Pad 6 (metallic)",
	"Pad 7 (halo)",
	"Pad 8 (sweep)",
	"FX 1 (rain)",
	"FX 2 (soundtrack)",
	"FX 3 (crystal)",
	"FX 4 (atmosphere)",
	"FX 5 (brightness)",
	"FX 6 (goblins)",
	"FX 7 (echoes)",
	"FX 8 (sci-fi)",
	"Sitar",
	"Banjo",
	"Shamisen",
	"Koto",
	"Kalimba",
	"Bagpipe",
	"Fiddle",
	"Shanai",
	"Tinkle Bell",
	"Agogo",
	"Steel Drums",
	"Woodblock",
	"Taiko Drum",
	"Melodic Tom",
	"Synth Drum",
	"Reverse Cymbal",
	"Guitar Fret Noise",
	"Breath Noise",
	"Seashore",
	"Bird Tweet",
	"Telephone Ring",
	"Helicopter",
	"Applause",
	"Gunshot",
};

} //namespace Name

} //namespace MIDI

