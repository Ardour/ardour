/*
    Copyright (C) 2014 Paul Davis

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

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/midi_port.h"
#include "ardour/midi_scene_change.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

MIDISceneChange::MIDISceneChange (int c, int b, int p)
	: _bank (b)
	, _program (p)
	, _channel (c & 0xf)
{
	if (_bank > 16384) {
		_bank = -1;
	}

	if (_program > 128) {
		_program = -1;
	}
}

MIDISceneChange::MIDISceneChange (const XMLNode& node, int version)
	: _bank (-1)
	, _program (-1)
	, _channel (-1)
{
	set_state (node, version);
}

MIDISceneChange::~MIDISceneChange ()
{
}

size_t
MIDISceneChange::get_bank_msb_message (uint8_t* buf, size_t size) const
{
	if (size < 3 || _bank < 0) {
		return 0;
	}

	buf[0] = 0xB0 | (_channel & 0xf);
	buf[1] = 0x0;
	buf[2] = (_bank >> 7) & 0x7f;

	return 3;
}

size_t
MIDISceneChange::get_bank_lsb_message (uint8_t* buf, size_t size) const
{
	if (size < 3 || _bank < 0) {
		return 0;
	}

	buf[0] = 0xB0 | (_channel & 0xf);
	buf[1] = 0x20;
	buf[2] = _bank & 0x7f;	

	return 3;
}

size_t
MIDISceneChange::get_program_message (uint8_t* buf, size_t size) const
{
	if (size < 2 || _program < 0) {
		return 0;
	}

	buf[0] = 0xC0 | (_channel & 0xf);
	buf[1] = _program & 0x7f;

	return 2;
}

XMLNode&
MIDISceneChange::get_state ()
{
	char buf[32];
	XMLNode* node = new XMLNode (SceneChange::xml_node_name);

	node->add_property (X_("type"), X_("MIDI"));
	snprintf (buf, sizeof (buf), "%d", (int) _program);
	node->add_property (X_("id"), id().to_s());
	snprintf (buf, sizeof (buf), "%d", (int) _program);
	node->add_property (X_("program"), buf);
	snprintf (buf, sizeof (buf), "%d", (int) _bank);
	node->add_property (X_("bank"), buf);
	snprintf (buf, sizeof (buf), "%d", (int) _channel);
	node->add_property (X_("channel"), buf);

	return *node;
}

int
MIDISceneChange::set_state (const XMLNode& node, int /* version-ignored */)
{
	if (!set_id (node)) {
		return -1;
	}

	const XMLProperty* prop;

	if ((prop = node.property (X_("program"))) == 0) {
		return -1;
	}
	_program = atoi (prop->value());

	if ((prop = node.property (X_("bank"))) == 0) {
		return -1;
	}
	_bank = atoi (prop->value());

	if ((prop = node.property (X_("channel"))) == 0) {
		return -1;
	}
	_channel = atoi (prop->value());

	return 0;
}
