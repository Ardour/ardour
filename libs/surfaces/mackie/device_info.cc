/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <cstdlib>
#include <cstring>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"

#include "ardour/filesystem_paths.h"

#include "device_info.h"

#include "i18n.h"

using namespace Mackie;
using namespace PBD;
using namespace ARDOUR;
using std::string;
using std::vector;

std::map<std::string,DeviceInfo> DeviceInfo::device_info;

DeviceInfo::DeviceInfo()
	: _strip_cnt (8)
	, _extenders (0)
	, _has_two_character_display (true)
	, _has_master_fader (true)
	, _has_timecode_display (true)
	, _has_global_controls (true)
	, _has_jog_wheel (true)
	, _has_touch_sense_faders (true)
	, _uses_logic_control_buttons (false)
	, _uses_ipmidi (false)
	, _no_handshake (false)
	, _has_meters (true)
	, _name (X_("Mackie Control Universal Pro"))
{
	mackie_control_buttons ();
}

DeviceInfo::~DeviceInfo()
{
}

std::string&
DeviceInfo::get_global_button_name(Button::ID id)
{
	std::map<Button::ID,GlobalButtonInfo>::iterator it;
	
	it = _global_buttons.find (id);
	if (it == _global_buttons.end ()) {
		_global_button_name = "";
		return _global_button_name;
	} else {
		return it->second.label;
	}
}

void
DeviceInfo::mackie_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();
	
	_global_buttons[Button::UserA] = GlobalButtonInfo ("Rear Panel User Switch 1", "user", 0x66);
	_global_buttons[Button::UserB] = GlobalButtonInfo ("Rear Panel User Switch 2", "user", 0x67);
	
	//TODO Implement "rear panel external control": a connection for a resistive 
	//TODO element expression pedal . Message: 0xb0 0x2e 0xVV where 0xVV = external 
	//TODO controller position value (0x00 to 0x7f)
	
	_strip_buttons[Button::RecEnable] = StripButtonInfo (0x0, "Rec");
	_strip_buttons[Button::FaderTouch] = StripButtonInfo (0xe0, "Fader Touch");
}

void
DeviceInfo::logic_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();
	
	_global_buttons[Button::UserA] = GlobalButtonInfo ("User Switch A", "user", 0x66);
	_global_buttons[Button::UserB] = GlobalButtonInfo ("User Switch B", "user", 0x67);

	_strip_buttons[Button::RecEnable] = StripButtonInfo (0x0, "Rec/Rdy");
	_strip_buttons[Button::FaderTouch] = StripButtonInfo (0x68, "Fader Touch");
}

void
DeviceInfo::shared_buttons ()
{
	_global_buttons[Button::Track] = GlobalButtonInfo ("Track", "assignment", 0x28);
	_global_buttons[Button::Send] = GlobalButtonInfo ("Send", "assignment", 0x29);
	_global_buttons[Button::Pan] = GlobalButtonInfo ("Pan/Surround", "assignment", 0x2a);
	_global_buttons[Button::Plugin] = GlobalButtonInfo ("Plugin", "assignment", 0x2b);
	_global_buttons[Button::Eq] = GlobalButtonInfo ("Eq", "assignment", 0x2c);
	_global_buttons[Button::Dyn] = GlobalButtonInfo ("Instrument", "assignment", 0x2d);
	
	_global_buttons[Button::Left] = GlobalButtonInfo ("Bank Left", "bank", 0x2e);
	_global_buttons[Button::Right] = GlobalButtonInfo ("Bank Right", "bank", 0x2f);
	_global_buttons[Button::ChannelLeft] = GlobalButtonInfo ("Channel Left", "bank", 0x30);
	_global_buttons[Button::ChannelRight] = GlobalButtonInfo ("Channel Right", "bank", 0x31);
	_global_buttons[Button::Flip] = GlobalButtonInfo ("Flip", "assignment", 0x32);
	_global_buttons[Button::View] = GlobalButtonInfo ("Global View", "global view", 0x33);

	_global_buttons[Button::NameValue] = GlobalButtonInfo ("Name/Value", "display", 0x34);
	_global_buttons[Button::TimecodeBeats] = GlobalButtonInfo ("Timecode/Beats", "display", 0x35);
	
	_global_buttons[Button::F1] = GlobalButtonInfo ("F1", "function select", 0x36);
	_global_buttons[Button::F2] = GlobalButtonInfo ("F2", "function select", 0x37);
	_global_buttons[Button::F3] = GlobalButtonInfo ("F3", "function select", 0x38);
	_global_buttons[Button::F4] = GlobalButtonInfo ("F4", "function select", 0x39);
	_global_buttons[Button::F5] = GlobalButtonInfo ("F5", "function select", 0x3a);
	_global_buttons[Button::F6] = GlobalButtonInfo ("F6", "function select", 0x3b);
	_global_buttons[Button::F7] = GlobalButtonInfo ("F7", "function select", 0x3c);
	_global_buttons[Button::F8] = GlobalButtonInfo ("F8", "function select", 0x3d);
	
	_global_buttons[Button::MidiTracks] = GlobalButtonInfo ("MIDI Tracks", "global view", 0x3e);
	_global_buttons[Button::Inputs] = GlobalButtonInfo ("Inputs", "global view", 0x3f);
	_global_buttons[Button::AudioTracks] = GlobalButtonInfo ("Audio Tracks", "global view", 0x40);
	_global_buttons[Button::AudioInstruments] = GlobalButtonInfo ("Audio Instruments", "global view", 0x41);
	_global_buttons[Button::Aux] = GlobalButtonInfo ("Aux", "global view", 0x42);
	_global_buttons[Button::Busses] = GlobalButtonInfo ("Busses", "global view", 0x43);
	_global_buttons[Button::Outputs] = GlobalButtonInfo ("Outputs", "global view", 0x44);
	_global_buttons[Button::User] = GlobalButtonInfo ("User", "global view", 0x45);
	
	_global_buttons[Button::Shift] = GlobalButtonInfo ("Shift", "modifiers", 0x46);
	_global_buttons[Button::Option] = GlobalButtonInfo ("Option", "modifiers", 0x47);
	_global_buttons[Button::Ctrl] = GlobalButtonInfo ("Ctrl", "modifiers", 0x48);
	_global_buttons[Button::CmdAlt] = GlobalButtonInfo ("Cmd/Alt", "modifiers", 0x49);
	
	_global_buttons[Button::Read] = GlobalButtonInfo ("Read/Off", "automation", 0x4a);
	_global_buttons[Button::Write] = GlobalButtonInfo ("Write", "automation", 0x4b);
	_global_buttons[Button::Trim] = GlobalButtonInfo ("Trim", "automation", 0x4c);
	_global_buttons[Button::Touch] = GlobalButtonInfo ("Touch", "automation", 0x4d);
	_global_buttons[Button::Latch] = GlobalButtonInfo ("Latch", "automation", 0x4e);
	_global_buttons[Button::Grp] = GlobalButtonInfo ("Group", "automation", 0x4f);
	
	_global_buttons[Button::Save] = GlobalButtonInfo ("Save", "utilities", 0x50);
	_global_buttons[Button::Undo] = GlobalButtonInfo ("Undo", "utilities", 0x51);
	_global_buttons[Button::Cancel] = GlobalButtonInfo ("Cancel", "utilities", 0x52);
	_global_buttons[Button::Enter] = GlobalButtonInfo ("Enter", "utilities", 0x53);

	_global_buttons[Button::Marker] = GlobalButtonInfo ("Marker", "transport", 0x54);
	_global_buttons[Button::Nudge] = GlobalButtonInfo ("Nudge", "transport", 0x55);
	_global_buttons[Button::Loop] = GlobalButtonInfo ("Cycle", "transport", 0x56);
	_global_buttons[Button::Drop] = GlobalButtonInfo ("Drop", "transport", 0x57);
	_global_buttons[Button::Replace] = GlobalButtonInfo ("Replace", "transport", 0x58);
	_global_buttons[Button::Click] = GlobalButtonInfo ("Click", "transport", 0x59);
	_global_buttons[Button::Solo] = GlobalButtonInfo ("Solo", "transport", 0x5a);
	
	_global_buttons[Button::Rewind] = GlobalButtonInfo ("Rewind", "transport", 0x5b);
	_global_buttons[Button::Ffwd] = GlobalButtonInfo ("Fast Fwd", "transport", 0x5c);
	_global_buttons[Button::Stop] = GlobalButtonInfo ("Stop", "transport", 0x5d);
	_global_buttons[Button::Play] = GlobalButtonInfo ("Play", "transport", 0x5e);
	_global_buttons[Button::Record] = GlobalButtonInfo ("Record", "transport", 0x5f);
	
	_global_buttons[Button::CursorUp] = GlobalButtonInfo ("Cursor Up", "cursor", 0x60);
	_global_buttons[Button::CursorDown] = GlobalButtonInfo ("Cursor Down", "cursor", 0x61);
	_global_buttons[Button::CursorLeft] = GlobalButtonInfo ("Cursor Left", "cursor", 0x62);
	_global_buttons[Button::CursorRight] = GlobalButtonInfo ("Cursor Right", "cursor", 0x63);
	_global_buttons[Button::Zoom] = GlobalButtonInfo ("Zoom", "cursor", 0x64);
	_global_buttons[Button::Scrub] = GlobalButtonInfo ("Scrub", "cursor", 0x65);

	_strip_buttons[Button::Solo] = StripButtonInfo (0x08, "Solo");
	_strip_buttons[Button::Mute] = StripButtonInfo (0x10, "Mute");
	_strip_buttons[Button::Select] = StripButtonInfo (0x18, "Select");
	_strip_buttons[Button::VSelect] = StripButtonInfo (0x20, "V-Select");
}

int
DeviceInfo::set_state (const XMLNode& node, int /* version */)
{
	const XMLProperty* prop;
	const XMLNode* child;

	if (node.name() != "MackieProtocolDevice") {
		return -1;
	}

	/* name is mandatory */
 
	if ((child = node.child ("Name")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_name = prop->value();
		} else {
			return -1;
		}
	}

	/* strip count is mandatory */

	if ((child = node.child ("Strips")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			if ((_strip_cnt = atoi (prop->value())) == 0) {
				_strip_cnt = 8;
			}
		}
	} else {
		return -1;
	}

	if ((child = node.child ("Extenders")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			if ((_extenders = atoi (prop->value())) == 0) {
				_extenders = 0;
			}
		}
	}

	if ((child = node.child ("TwoCharacterDisplay")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_two_character_display = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("MasterFader")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_master_fader = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("TimecodeDisplay")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_timecode_display = string_is_affirmative (prop->value());
		}
	} else {
		_has_timecode_display = false;
	}

	if ((child = node.child ("GlobalControls")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_global_controls = string_is_affirmative (prop->value());
		}
	} else {
		_has_global_controls = false;
	}

	if ((child = node.child ("JogWheel")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_jog_wheel = string_is_affirmative (prop->value());
		}
	} else {
		_has_jog_wheel = false;
	}

	if ((child = node.child ("TouchSenseFaders")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_touch_sense_faders = string_is_affirmative (prop->value());
		}
	} else {
		_has_touch_sense_faders = false;
	}

	if ((child = node.child ("UsesIPMIDI")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_uses_ipmidi = string_is_affirmative (prop->value());
		}
	} else {
		_uses_ipmidi = false;
	}

	if ((child = node.child ("NoHandShake")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_no_handshake = string_is_affirmative (prop->value());
		}
	} else {
		_no_handshake = false;
	}

	if ((child = node.child ("HasMeters")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_meters = string_is_affirmative (prop->value());
		}
	} else {
		_has_meters = true;
	}

	if ((child = node.child ("LogicControlButtons")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_uses_logic_control_buttons = string_is_affirmative (prop->value());

			if (_uses_logic_control_buttons) {
				logic_control_buttons();
			} else {
				mackie_control_buttons ();
			}
		}
	}

	if ((child = node.child ("Buttons")) != 0) {
		XMLNodeConstIterator i;
		const XMLNodeList& nlist (child->children());

		for (i = nlist.begin(); i != nlist.end(); ++i) {
			if ((*i)->name() == "GlobalButton") {
				if ((prop = (*i)->property ("name")) != 0) {
					int id = Button::name_to_id (prop->value());
					if (id >= 0) {
						Button::ID bid = (Button::ID) id;
						if ((prop = (*i)->property ("id")) != 0) {
							int val = strtol (prop->value().c_str(), 0, 0);
							std::map<Button::ID,GlobalButtonInfo>::iterator b = _global_buttons.find (bid);
							if (b != _global_buttons.end()) {
								b->second.id = val;
								
								if ((prop = (*i)->property ("label")) != 0) {
									b->second.label = prop->value();
								}
							}
						}
					}
					
				}
				
			} else if ((*i)->name() == "StripButton") {
				if ((prop = (*i)->property ("name")) != 0) {
					int id = Button::name_to_id (prop->value());
					if (id >= 0) {
						Button::ID bid = (Button::ID) id;
						if ((prop = (*i)->property ("baseid")) != 0) {
							int val = strtol (prop->value().c_str(), 0, 0);
							std::map<Button::ID,StripButtonInfo>::iterator b = _strip_buttons.find (bid);
							if (b != _strip_buttons.end()) {
								b->second.base_id = val;
							}
						}
					}
					
				}
				
			}
		}
	}

	return 0;
}

const string&
DeviceInfo::name() const
{
	return _name;
}

uint32_t
DeviceInfo::strip_cnt() const
{
	return _strip_cnt;
}

uint32_t
DeviceInfo::extenders() const
{
	return _extenders;
}

bool
DeviceInfo::has_master_fader() const
{
	return _has_master_fader;
}

bool
DeviceInfo::has_meters() const
{
	return _has_meters;
}

bool
DeviceInfo::has_two_character_display() const
{
	return _has_two_character_display;
}

bool
DeviceInfo::has_timecode_display () const
{
	return _has_timecode_display;
}

bool
DeviceInfo::uses_ipmidi () const
{
	return _uses_ipmidi;
}

bool
DeviceInfo::has_global_controls () const
{
	return _has_global_controls;
}

bool
DeviceInfo::has_jog_wheel () const
{
	return _has_jog_wheel;
}

bool
DeviceInfo::no_handshake () const
{
	return _no_handshake;
}

bool
DeviceInfo::has_touch_sense_faders () const
{
	return _has_touch_sense_faders;
}

static const char * const devinfo_env_variable_name = "ARDOUR_MCP_PATH";
static const char* const devinfo_dir_name = "mcp";
static const char* const devinfo_suffix = ".device";

static SearchPath
devinfo_search_path ()
{
	bool devinfo_path_defined = false;
        sys::path spath_env (Glib::getenv (devinfo_env_variable_name, devinfo_path_defined));

	if (devinfo_path_defined) {
		return spath_env;
	}

	SearchPath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(devinfo_dir_name);

	return spath;
}

static bool
devinfo_filter (const string &str, void */*arg*/)
{
	return (str.length() > strlen(devinfo_suffix) &&
		str.find (devinfo_suffix) == (str.length() - strlen (devinfo_suffix)));
}

void
DeviceInfo::reload_device_info ()
{
	DeviceInfo di;
	vector<string> s;
	vector<string *> *devinfos;
	PathScanner scanner;
	SearchPath spath (devinfo_search_path());

	devinfos = scanner (spath.to_string(), devinfo_filter, 0, false, true);
	device_info.clear ();

	if (!devinfos) {
		error << "No MCP device info files found using " << spath.to_string() << endmsg;
		std::cerr << "No MCP device info files found using " << spath.to_string() << std::endl;
		return;
	}

	if (devinfos->empty()) {
		error << "No MCP device info files found using " << spath.to_string() << endmsg;
		std::cerr << "No MCP device info files found using " << spath.to_string() << std::endl;
		return;
	}

	for (vector<string*>::iterator i = devinfos->begin(); i != devinfos->end(); ++i) {
		string fullpath = *(*i);

		XMLTree tree;


		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		XMLNode* root = tree.root ();
		if (!root) {
			continue;
		}

		if (di.set_state (*root, 3000) == 0) { /* version is ignored for now */
			device_info[di.name()] = di;
		}
	}

	delete devinfos;
}

std::ostream& operator<< (std::ostream& os, const Mackie::DeviceInfo& di)
{
	os << di.name() << ' ' 
	   << di.strip_cnt() << ' '
	   << di.extenders() << ' '
		;
	return os;
}
