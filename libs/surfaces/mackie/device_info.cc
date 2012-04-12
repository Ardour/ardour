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
	, _name (X_("Mackie Control Universal Pro"))
{
	
}

DeviceInfo::~DeviceInfo()
{
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

static const char * const devinfo_env_variable_name = "ARDOUR_MCP_DEVINFO_PATH";
static const char* const devinfo_dir_name = "mcp_devices";
static const char* const devinfo_suffix = ".xml";

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
			std::cerr << di << '\n';
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
