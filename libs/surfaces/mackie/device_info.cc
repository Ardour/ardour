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
	, _has_segmented_display (false)
	, _has_timecode_display (true)
	, _has_global_controls (true)
	, _has_jog_wheel (true)
	, _has_touch_sense_faders (true)
	, _uses_logic_control_buttons (false)
	, _name (X_("Mackie Control Universal Pro"))
{
	mackie_control_buttons ();
}

DeviceInfo::~DeviceInfo()
{
}

void
DeviceInfo::mackie_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();

	_global_buttons[Button::Edit] = GlobalButtonInfo ("edit", "none", 0x33);

	_global_buttons[Button::F9] = GlobalButtonInfo ("F9", "none", 0x3e);
	_global_buttons[Button::F10] = GlobalButtonInfo ("F10", "none", 0x3f);
	_global_buttons[Button::F11] = GlobalButtonInfo ("F11", "none", 0x40);
	_global_buttons[Button::F12] = GlobalButtonInfo ("F12", "none", 0x41);
	_global_buttons[Button::F13] = GlobalButtonInfo ("F13", "none", 0x42);
	_global_buttons[Button::F14] = GlobalButtonInfo ("F14", "none", 0x43);
	_global_buttons[Button::F15] = GlobalButtonInfo ("F15", "none", 0x44);
	_global_buttons[Button::F16] = GlobalButtonInfo ("F16", "none", 0x45);
	_global_buttons[Button::Ctrl] = GlobalButtonInfo ("ctrl", "modifiers", 0x46);
	_global_buttons[Button::Option] = GlobalButtonInfo ("option", "modifiers", 0x47);
	_global_buttons[Button::Snapshot] = GlobalButtonInfo ("snapshot", "modifiers", 0x48);
	_global_buttons[Button::Shift] = GlobalButtonInfo ("shift", "modifiers", 0x49);
	_global_buttons[Button::Read] = GlobalButtonInfo ("read", "automation", 0x4a);
	_global_buttons[Button::Write] = GlobalButtonInfo ("write", "automation", 0x4b);
	_global_buttons[Button::Undo] = GlobalButtonInfo ("undo", "functions", 0x4c);
	_global_buttons[Button::Save] = GlobalButtonInfo ("save", "automation", 0x4d);
	_global_buttons[Button::Touch] = GlobalButtonInfo ("touch", "automation", 0x4e);
	_global_buttons[Button::Redo] = GlobalButtonInfo ("redo", "functions", 0x4f);
	_global_buttons[Button::FdrGroup] = GlobalButtonInfo ("fader group", "functions", 0x50);
	_global_buttons[Button::ClearSolo] = GlobalButtonInfo ("clear solo", "functions", 0x51);
	_global_buttons[Button::Cancel] = GlobalButtonInfo ("cancel", "functions", 0x52);
	_global_buttons[Button::Marker] = GlobalButtonInfo ("marker", "functions", 0x53);
	_global_buttons[Button::Mixer] = GlobalButtonInfo ("mixer", "transport", 0x54);
	_global_buttons[Button::FrmLeft] = GlobalButtonInfo ("frm left", "transport", 0x55);
	_global_buttons[Button::FrmRight] = GlobalButtonInfo ("frm right", "transport", 0x56);
	_global_buttons[Button::End] = GlobalButtonInfo ("end", "transport", 0x57);
	_global_buttons[Button::PunchIn] = GlobalButtonInfo ("punch in", "transport", 0x58);
	_global_buttons[Button::PunchOut] = GlobalButtonInfo ("punch out", "transport", 0x59);
	_global_buttons[Button::Loop] = GlobalButtonInfo ("loop", "transport", 0x59);
	_global_buttons[Button::Home] = GlobalButtonInfo ("home", "transport", 0x5a);

	_strip_buttons[Button::FaderTouch] = StripButtonInfo (0xe0, "fader touch");
}

void
DeviceInfo::logic_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();

	_global_buttons[Button::View] = GlobalButtonInfo ("view", "view", 0x33);

	_global_buttons[Button::MidiTracks] = GlobalButtonInfo ("miditracks", "view", 0x3e);
	_global_buttons[Button::Inputs] = GlobalButtonInfo ("inputs", "view", 0x3f);
	_global_buttons[Button::AudioTracks] = GlobalButtonInfo ("audiotracks", "view", 0x40);
	_global_buttons[Button::AudioInstruments] = GlobalButtonInfo ("audio instruments", "view", 0x41);
	_global_buttons[Button::Aux] = GlobalButtonInfo ("aux", "view", 0x42);
	_global_buttons[Button::Busses] = GlobalButtonInfo ("busses", "view", 0x43);
	_global_buttons[Button::Outputs] = GlobalButtonInfo ("outputs", "view", 0x44);
	_global_buttons[Button::User] = GlobalButtonInfo ("user", "view", 0x45);
	_global_buttons[Button::Shift] = GlobalButtonInfo ("shift", "modifiers", 0x46);
	_global_buttons[Button::Option] = GlobalButtonInfo ("option", "modifiers", 0x47);
	_global_buttons[Button::Ctrl] = GlobalButtonInfo ("ctrl", "modifiers", 0x48);
	_global_buttons[Button::CmdAlt] = GlobalButtonInfo ("cmdalt", "modifiers", 0x49);
	_global_buttons[Button::Read] = GlobalButtonInfo ("read", "automation", 0x4a);
	_global_buttons[Button::Write] = GlobalButtonInfo ("write", "automation", 0x4b);
	_global_buttons[Button::Trim] = GlobalButtonInfo ("trim", "automation", 0x4c);
	_global_buttons[Button::Touch] = GlobalButtonInfo ("touch", "functions", 0x4d);
	_global_buttons[Button::Latch] = GlobalButtonInfo ("latch", "functions", 0x4e);
	_global_buttons[Button::Grp] = GlobalButtonInfo ("group", "functions", 0x4f);
	_global_buttons[Button::Save] = GlobalButtonInfo ("save", "functions", 0x50);
	_global_buttons[Button::Undo] = GlobalButtonInfo ("undo", "functions", 0x51);
	_global_buttons[Button::Cancel] = GlobalButtonInfo ("cancel", "transport", 0x52);
	_global_buttons[Button::Enter] = GlobalButtonInfo ("enter right", "transport", 0x53);
	_global_buttons[Button::Marker] = GlobalButtonInfo ("marker", "transport", 0x54);
	_global_buttons[Button::Nudge] = GlobalButtonInfo ("nudge", "transport", 0x55);
	_global_buttons[Button::Loop] = GlobalButtonInfo ("cycle", "transport", 0x56);
	_global_buttons[Button::Drop] = GlobalButtonInfo ("drop", "transport", 0x57);
	_global_buttons[Button::Replace] = GlobalButtonInfo ("replace", "transport", 0x58);
	_global_buttons[Button::Click] = GlobalButtonInfo ("click", "transport", 0x59);
	_global_buttons[Button::Solo] = GlobalButtonInfo ("solo", "transport", 0x5a);

	_strip_buttons[Button::FaderTouch] = StripButtonInfo (0x68, "fader touch");
}

void
DeviceInfo::shared_buttons ()
{
	_global_buttons[Button::Track] = GlobalButtonInfo ("track", "assignment", 0x28);
	_global_buttons[Button::Send] = GlobalButtonInfo ("send", "assignment", 0x29);
	_global_buttons[Button::Pan] = GlobalButtonInfo ("pan", "assignment", 0x2a);
	_global_buttons[Button::Plugin] = GlobalButtonInfo ("plugin", "assignment", 0x2b);
	_global_buttons[Button::Eq] = GlobalButtonInfo ("eq", "assignment", 0x2c);
	_global_buttons[Button::Dyn] = GlobalButtonInfo ("dyn", "assignment", 0x2d);
	_global_buttons[Button::Left] = GlobalButtonInfo ("left", "bank", 0x2e);
	_global_buttons[Button::Right] = GlobalButtonInfo ("right", "bank", 0x2f);
	_global_buttons[Button::ChannelLeft] = GlobalButtonInfo ("channelleft", "bank", 0x30);
	_global_buttons[Button::ChannelRight] = GlobalButtonInfo ("channelright", "bank", 0x31);
	_global_buttons[Button::Flip] = GlobalButtonInfo ("flip", "none", 0x32);

	_global_buttons[Button::NameValue] = GlobalButtonInfo ("name/value", "display", 0x34);
	_global_buttons[Button::TimecodeBeats] = GlobalButtonInfo ("timecode/beats", "display", 0x35);
	_global_buttons[Button::F1] = GlobalButtonInfo ("F1", "none", 0x36);
	_global_buttons[Button::F2] = GlobalButtonInfo ("F2", "none", 0x37);
	_global_buttons[Button::F3] = GlobalButtonInfo ("F3", "none", 0x38);
	_global_buttons[Button::F4] = GlobalButtonInfo ("F4", "none", 0x39);
	_global_buttons[Button::F5] = GlobalButtonInfo ("F5", "none", 0x3a);
	_global_buttons[Button::F6] = GlobalButtonInfo ("F6", "none", 0x3b);
	_global_buttons[Button::F7] = GlobalButtonInfo ("F7", "none", 0x3c);
	_global_buttons[Button::F8] = GlobalButtonInfo ("F8", "none", 0x3d);

	_global_buttons[Button::Rewind] = GlobalButtonInfo ("rewind", "transport", 0x5b);
	_global_buttons[Button::Ffwd] = GlobalButtonInfo ("ffwd", "transport", 0x5c);
	_global_buttons[Button::Stop] = GlobalButtonInfo ("stop", "transport", 0x5d);
	_global_buttons[Button::Play] = GlobalButtonInfo ("play", "transport", 0x5e);
	_global_buttons[Button::Record] = GlobalButtonInfo ("record", "transport", 0x5f);
	_global_buttons[Button::CursorUp] = GlobalButtonInfo ("cursor up", "cursor", 0x60);
	_global_buttons[Button::CursorDown] = GlobalButtonInfo ("cursor down", "cursor", 0x61);
	_global_buttons[Button::CursorLeft] = GlobalButtonInfo ("cursor left", "cursor", 0x62);
	_global_buttons[Button::CursorRight] = GlobalButtonInfo ("cursor right", "cursor", 0x63);
	_global_buttons[Button::Zoom] = GlobalButtonInfo ("zoom", "none", 0x64);
	_global_buttons[Button::Scrub] = GlobalButtonInfo ("scrub", "none", 0x65);
	_global_buttons[Button::UserA] = GlobalButtonInfo ("user a", "user", 0x66);
	_global_buttons[Button::UserB] = GlobalButtonInfo ("user b", "user", 0x67);

	_strip_buttons[Button::RecEnable] = StripButtonInfo (0x0, "recenable");
	_strip_buttons[Button::Solo] = StripButtonInfo (0x08, "solo");
	_strip_buttons[Button::Mute] = StripButtonInfo (0x10, "mute");
	_strip_buttons[Button::Select] = StripButtonInfo (0x18, "select");
	_strip_buttons[Button::VSelect] = StripButtonInfo (0x20, "vselect");
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

	if ((child = node.child ("Strips")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			if ((_strip_cnt = atoi (prop->value())) == 0) {
				_strip_cnt = 8;
			}
		}
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

	if ((child = node.child ("DisplaySegments")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_segmented_display = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("TimecodeDisplay")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_timecode_display = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("GlobalControls")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_global_controls = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("JogWheel")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_jog_wheel = string_is_affirmative (prop->value());
		}
	}

	if ((child = node.child ("TouchSenseFaders")) != 0) {
		if ((prop = child->property ("value")) != 0) {
			_has_touch_sense_faders = string_is_affirmative (prop->value());
		}
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
DeviceInfo::has_two_character_display() const
{
	return _has_two_character_display;
}

bool
DeviceInfo::has_segmented_display() const
{
	return _has_segmented_display;
}

bool
DeviceInfo::has_timecode_display () const
{
	return _has_timecode_display;
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
DeviceInfo::has_touch_sense_faders () const
{
	return _has_touch_sense_faders;
}

static const char * const devinfo_env_variable_name = "ARDOUR_MCP_PATH";
static const char* const devinfo_dir_name = "mcp";
static const char* const devinfo_suffix = ".device";

static sys::path
system_devinfo_search_path ()
{
	bool devinfo_path_defined = false;
        sys::path spath_env (Glib::getenv (devinfo_env_variable_name, devinfo_path_defined));

	if (devinfo_path_defined) {
		return spath_env;
	}

	SearchPath spath (system_data_search_path());
	spath.add_subdirectory_to_paths(devinfo_dir_name);

	// just return the first directory in the search path that exists
	SearchPath::const_iterator i = std::find_if(spath.begin(), spath.end(), sys::exists);

	if (i == spath.end()) return sys::path();

	return *i;
}

static sys::path
user_devinfo_directory ()
{
	sys::path p(user_config_directory());
	p /= devinfo_dir_name;

	return p;
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
	SearchPath spath (system_devinfo_search_path());
	spath += user_devinfo_directory ();

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
