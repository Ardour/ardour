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
		commands->add_child_copy(*(event->to_xml()));
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
		_patch_midi_commands.push_back(*(new Event(*(*i))));
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
MIDINameDocument::set_state(const XMLNode & a_node)
{
}

XMLNode&
MIDINameDocument::get_state(void)
{
}


} //namespace Name

} //namespace MIDI

