/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstdlib>
#include <cstring>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/convert.h"
#include "pbd/stl_delete.h"

#include "ardour/filesystem_paths.h"

#include "device_info.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace US2400;

using std::string;
using std::vector;

std::map<std::string,DeviceInfo> DeviceInfo::device_info;

DeviceInfo::DeviceInfo()
	: _strip_cnt (8)
	, _extenders (3)
	, _master_position (0)
	, _has_two_character_display (false)
	, _has_master_fader (true)
	, _has_timecode_display (false)
	, _has_global_controls (true)
	, _has_jog_wheel (true)
	, _has_touch_sense_faders (true)
	, _uses_logic_control_buttons (false)
	, _no_handshake (false)
	, _has_meters (true)
	, _has_separate_meters (true)
	, _device_type (MCU)
	, _name (X_("US2400"))
{
	us2400_control_buttons ();
}

DeviceInfo::~DeviceInfo()
{
}

GlobalButtonInfo&
DeviceInfo::get_global_button(Button::ID id)
{
	GlobalButtonsInfo::iterator it;

	it = _global_buttons.find (id);

	return it->second;
}

std::string&
DeviceInfo::get_global_button_name(Button::ID id)
{
	GlobalButtonsInfo::iterator it;

	it = _global_buttons.find (id);
	if (it == _global_buttons.end ()) {
		_global_button_name = "";
		return _global_button_name;
	} else {
		return it->second.label;
	}
}

void
DeviceInfo::us2400_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();
}

void
DeviceInfo::logic_control_buttons ()
{
	_global_buttons.clear ();
	shared_buttons ();
}

void
DeviceInfo::shared_buttons ()
{
	/* US-2499 button notes:
	 * CHAN button sends nothing.  it inititates a dumb 0..127 knob mode for the 24 knobs
	 * PAN sends the regular pan/surround message.  this tells our strips to send the pan knob position
	 * AUX1-6  all send the same 0x29 + 0x21 message, I believe the surface uses this to captures knob info, somehow
	 */

	_global_buttons[Button::Pan] = GlobalButtonInfo ("Pan/Surround", "assignment", 0x2a);  // US-2400:  this is sent (on&off in one msg) from the Pan button

	_global_buttons[Button::Left] = GlobalButtonInfo ("Bank Left", "bank", 0x2e);
	_global_buttons[Button::Right] = GlobalButtonInfo ("Bank Right", "bank", 0x2f);

	_global_buttons[Button::Flip] = GlobalButtonInfo ("Flip", "assignment", 0x32);

	_global_buttons[Button::MstrSelect] = GlobalButtonInfo ("Mstr Select", "assignment", 0x48);

	_global_buttons[Button::F1] = GlobalButtonInfo ("F1", "function select", 0x36);
	_global_buttons[Button::F2] = GlobalButtonInfo ("F2", "function select", 0x37);
	_global_buttons[Button::F3] = GlobalButtonInfo ("F3", "function select", 0x38);
	_global_buttons[Button::F4] = GlobalButtonInfo ("F4", "function select", 0x39);
	_global_buttons[Button::F5] = GlobalButtonInfo ("F5", "function select", 0x3a);
	_global_buttons[Button::F6] = GlobalButtonInfo ("F6", "function select", 0x3b);


	_global_buttons[Button::Shift] = GlobalButtonInfo ("Shift", "modifiers", 0x46);
	_global_buttons[Button::Option] = GlobalButtonInfo ("Option", "modifiers", 0x47);  //There is no physical Option button, but US2400 sends Option+ track Solo == solo clear

	_global_buttons[Button::Drop] = GlobalButtonInfo ("Drop", "transport", 0x57);  // US-2400:  combined with ffwd/rew  to call IN/OUT

	_global_buttons[Button::Rewind] = GlobalButtonInfo ("Rewind", "transport", 0x5b);  // US-2400:   if "Drop" 0x57 is held, this is IN
	_global_buttons[Button::Ffwd] = GlobalButtonInfo ("Fast Fwd", "transport", 0x5c);  // US-2400:   if "Drop 0x57 is held, this is OUT
	_global_buttons[Button::Stop] = GlobalButtonInfo ("Stop", "transport", 0x5d);
	_global_buttons[Button::Play] = GlobalButtonInfo ("Play", "transport", 0x5e);
	_global_buttons[Button::Record] = GlobalButtonInfo ("Record", "transport", 0x5f);

	_global_buttons[Button::Scrub] = GlobalButtonInfo ("Scrub", "cursor", 0x65);

	_strip_buttons[Button::Solo] = StripButtonInfo (0x08, "Solo");  //combined with Option" to do solo clear
	_strip_buttons[Button::Mute] = StripButtonInfo (0x10, "Mute");
	_strip_buttons[Button::Select] = StripButtonInfo (0x18, "Select");

	_strip_buttons[Button::FaderTouch] = StripButtonInfo (0x68, "Fader Touch");

	_global_buttons[Button::MasterFaderTouch] = GlobalButtonInfo ("Master Fader Touch", "master", 0x70);
}

int
DeviceInfo::set_state (const XMLNode& node, int /* version */)
{
	const XMLNode* child;

	if (node.name() != "US-2400Device") {
		return -1;
	}

	if ((child = node.child ("LogicControlButtons")) != 0) {
		if (child->get_property ("value", _uses_logic_control_buttons)) {
			if (_uses_logic_control_buttons) {
				logic_control_buttons ();
			} else {
				us2400_control_buttons ();
			}
		}
	}

	if ((child = node.child ("Buttons")) != 0) {
		XMLNodeConstIterator i;
		const XMLNodeList& nlist (child->children());

		std::string name;
		for (i = nlist.begin(); i != nlist.end(); ++i) {
			if ((*i)->name () == "GlobalButton") {
				if ((*i)->get_property ("name", name)) {
					int id = Button::name_to_id (name);
					if (id >= 0) {
						Button::ID bid = (Button::ID)id;
						int32_t id;
						if ((*i)->get_property ("id", id)) {
							std::map<Button::ID, GlobalButtonInfo>::iterator b = _global_buttons.find (bid);
							if (b != _global_buttons.end ()) {
								b->second.id = id;
								(*i)->get_property ("label", b->second.label);
							}
						}
					}
				}
			} else if ((*i)->name () == "StripButton") {
				if ((*i)->get_property ("name", name)) {
					int id = Button::name_to_id (name);
					if (id >= 0) {
						Button::ID bid = (Button::ID)id;
						int32_t base_id;
						if ((*i)->get_property ("baseid", base_id)) {
							std::map<Button::ID, StripButtonInfo>::iterator b = _strip_buttons.find (bid);
							if (b != _strip_buttons.end ()) {
								b->second.base_id = base_id;
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

uint32_t
DeviceInfo::master_position() const
{
	return _master_position;
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
DeviceInfo::has_separate_meters() const
{
	return _has_separate_meters;
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

static Searchpath
devinfo_search_path ()
{
	bool devinfo_path_defined = false;
	std::string spath_env (Glib::getenv (devinfo_env_variable_name, devinfo_path_defined));

	if (devinfo_path_defined) {
		return spath_env;
	}

	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(devinfo_dir_name);

	return spath;
}

static bool
devinfo_filter (const string &str, void* /*arg*/)
{
	return (str.length() > strlen(devinfo_suffix) &&
		str.find (devinfo_suffix) == (str.length() - strlen (devinfo_suffix)));
}

void
DeviceInfo::reload_device_info ()
{
	vector<string> s;
	vector<string> devinfos;
	Searchpath spath (devinfo_search_path());

	find_files_matching_filter (devinfos, spath, devinfo_filter, 0, false, true);
	device_info.clear ();

	if (devinfos.empty()) {
		error << "No MCP device info files found using " << spath.to_string() << endmsg;
		std::cerr << "No MCP device info files found using " << spath.to_string() << std::endl;
		return;
	}

	for (vector<string>::iterator i = devinfos.begin(); i != devinfos.end(); ++i) {
		string fullpath = *i;
		DeviceInfo di; // has to be initial every loop or info from last added.

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
}

std::ostream& operator<< (std::ostream& os, const US2400::DeviceInfo& di)
{
	os << di.name() << ' '
	   << di.strip_cnt() << ' '
	   << di.extenders() << ' '
	   << di.master_position() << ' '
		;
	return os;
}
