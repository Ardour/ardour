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

#include "midi++/midnam_patch.h"
#include <algorithm>

namespace MIDI
{

namespace Name
{

XMLNode&
Patch::get_state (void)
{
	XMLNode* node = new XMLNode("Patch");
	node->add_property("Number", _number);
	node->add_property("Name",   _name);
	XMLNode* commands = node->add_child("PatchMIDICommands");
	for (PatchMidiCommands::const_iterator event = _patch_midi_commands.begin();
	    event != _patch_midi_commands.end();
	    ++event) {
		commands->add_child_copy(*((((Evoral::MIDIEvent&)*event)).to_xml()));
	}

	return *node;
}

int
Patch::set_state (const XMLNode& node)
{
	assert(node.name() == "Patch");
	_number = node.property("Number")->value();
	_name   = node.property("Name")->value();
	XMLNode* commands = node.child("PatchMIDICommands");
	assert(commands);
	const XMLNodeList events = commands->children();
	for (XMLNodeList::const_iterator i = events.begin(); i != events.end(); ++i) {
		_patch_midi_commands.push_back(*(new Evoral::MIDIEvent(*(*i))));
	}

	return 0;
}

XMLNode&
Note::get_state (void)
{
	XMLNode* node = new XMLNode("Note");
	node->add_property("Number", _number);
	node->add_property("Name",   _name);

	return *node;
}

int
Note::set_state (const XMLNode& node)
{
	assert(node.name() == "Note");
	_number = node.property("Number")->value();
	_name   = node.property("Name")->value();

	return 0;
}

XMLNode&
NoteNameList::get_state (void)
{
	XMLNode* node = new XMLNode("NoteNameList");
	node->add_property("Name",   _name);

	return *node;
}

int
NoteNameList::set_state (const XMLNode& node)
{
	assert(node.name() == "NoteNameList");
	_name   = node.property("Name")->value();

	boost::shared_ptr<XMLSharedNodeList> notes =
					node.find("//Note");
	for (XMLSharedNodeList::const_iterator i = notes->begin(); i != notes->end(); ++i) {
		Note note;
		note.set_state(*(*i));
		_notes.push_back(note);
	}
	
	return 0;
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
		patch_name_list->add_child_nocopy(patch->get_state());
	}

	return *node;
}

int
PatchBank::set_state (const XMLNode& node)
{
	assert(node.name() == "PatchBank");
	_name   = node.property("Name")->value();
	XMLNode* patch_name_list = node.child("PatchNameList");
	assert(patch_name_list);
	const XMLNodeList patches = patch_name_list->children();
	for (XMLNodeList::const_iterator i = patches.begin(); i != patches.end(); ++i) {
		Patch patch;
		patch.set_state(*(*i));
		_patch_name_list.push_back(patch);
	}

	return 0;
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
		node->add_child_nocopy(patch_bank->get_state());
	}

	return *node;
}

int
ChannelNameSet::set_state (const XMLNode& node)
{
	assert(node.name() == "ChannelNameSet");
	_name   = node.property("Name")->value();
	const XMLNodeList children = node.children();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		XMLNode* node = *i;
		assert(node);
		if (node->name() == "AvailableForChannels") {
			boost::shared_ptr<XMLSharedNodeList> channels =
				node->find("//AvailableChannel[@Available = 'true']/@Channel");
			for(XMLSharedNodeList::const_iterator i = channels->begin();
			    i != channels->end();
			    ++i) {
				_available_for_channels.insert(atoi((*i)->attribute_value().c_str()));
			}
		}

		if (node->name() == "PatchBank") {
			PatchBank bank;
			bank.set_state(*node);
			_patch_banks.push_back(bank);
		}
	}

	return 0;
}

int
CustomDeviceMode::set_state(const XMLNode& a_node)
{
	assert(a_node.name() == "CustomDeviceNode");
	boost::shared_ptr<XMLSharedNodeList> channel_name_set_assignments =
		a_node.find("//ChannelNameSetAssign");
	for(XMLSharedNodeList::const_iterator i = channel_name_set_assignments->begin();
	    i != channel_name_set_assignments->end();
	    ++i) {
		int channel = atoi((*i)->property("Channel")->value().c_str());
		string name_set = (*i)->property("NameSet")->value();
		assert( 1 <= channel && channel <= 16 );
		_channel_name_set_assignments[channel -1] = name_set;
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

int
MasterDeviceNames::set_state(const XMLNode& a_node)
{
	// Manufacturer
	boost::shared_ptr<XMLSharedNodeList> manufacturer = a_node.find("//Manufacturer");
	assert(manufacturer->size() == 1);
	_manufacturer = manufacturer->front()->content();

	// Models
	boost::shared_ptr<XMLSharedNodeList> models = a_node.find("//Model");
	assert(models->size() >= 1);
	for (XMLSharedNodeList::iterator i = models->begin();
	     i != models->end();
	     ++i) {
		_models.push_back((*i)->content());
	}

	// CustomDeviceModes
	boost::shared_ptr<XMLSharedNodeList> custom_device_modes = a_node.find("//CustomDeviceMode");
	for (XMLSharedNodeList::iterator i = custom_device_modes->begin();
	     i != custom_device_modes->end();
	     ++i) {
		CustomDeviceMode custom_device_mode;
		custom_device_mode.set_state(*(*i));
		_custom_device_modes.push_back(custom_device_mode);
	}

	// ChannelNameSets
	boost::shared_ptr<XMLSharedNodeList> channel_name_sets = a_node.find("//ChannelNameSet");
	for (XMLSharedNodeList::iterator i = channel_name_sets->begin();
	     i != channel_name_sets->end();
	     ++i) {
		ChannelNameSet channel_name_set;
		channel_name_set.set_state(*(*i));
		_channel_name_sets.push_back(channel_name_set);
	}

	// NoteNameLists
	boost::shared_ptr<XMLSharedNodeList> note_name_lists = a_node.find("//NoteNameList");
	for (XMLSharedNodeList::iterator i = note_name_lists->begin();
	     i != note_name_lists->end();
	     ++i) {
		NoteNameList note_name_list;
		note_name_list.set_state(*(*i));
		_note_name_lists.push_back(note_name_list);
	}

	return 0;
}

XMLNode&
MasterDeviceNames::get_state(void)
{
	static XMLNode nothing("<nothing>");
	return nothing;
}

int
MIDINameDocument::set_state(const XMLNode& a_node)
{
	// Author
	boost::shared_ptr<XMLSharedNodeList> author = a_node.find("//Author");
	assert(author->size() == 1);
	_author = author->front()->content();
	
	// MasterDeviceNames
	boost::shared_ptr<XMLSharedNodeList> master_device_names_list = a_node.find("//MasterDeviceNames");
	for (XMLSharedNodeList::iterator i = master_device_names_list->begin();
	     i != master_device_names_list->end();
	     ++i) {
		MasterDeviceNames master_device_names;
		master_device_names.set_state(*(*i));
		_master_device_names_list.push_back(master_device_names);
	}
	
	return 0;
}

XMLNode&
MIDINameDocument::get_state(void)
{
	static XMLNode nothing("<nothing>");
	return nothing;
}


} //namespace Name

} //namespace MIDI

