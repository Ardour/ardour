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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/stl_delete.h"
#include "pbd/replace_all.h"

#include "ardour/filesystem_paths.h"

#include "mackie_control_protocol.h"
#include "device_profile.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace Mackie;

using std::string;
using std::vector;

std::map<std::string,DeviceProfile> DeviceProfile::device_profiles;
const std::string DeviceProfile::edited_indicator (" (edited)");
const std::string DeviceProfile::default_profile_name ("User");

DeviceProfile::DeviceProfile (const string& n)
	: _name (n)
	, edited (false)
{
}

DeviceProfile::~DeviceProfile()
{
}

static const char * const devprofile_env_variable_name = "ARDOUR_MCP_PATH";
static const char* const devprofile_dir_name = "mcp";
static const char* const devprofile_suffix = ".profile";

static Searchpath
devprofile_search_path ()
{
	bool devprofile_path_defined = false;
        std::string spath_env (Glib::getenv (devprofile_env_variable_name, devprofile_path_defined));

	if (devprofile_path_defined) {
		return spath_env;
	}

	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(devprofile_dir_name);

	return spath;
}

static std::string
user_devprofile_directory ()
{
	return Glib::build_filename (user_config_directory(), devprofile_dir_name);
}

static bool
devprofile_filter (const string &str, void* /*arg*/)
{
	return (str.length() > strlen(devprofile_suffix) &&
		str.find (devprofile_suffix) == (str.length() - strlen (devprofile_suffix)));
}

void
DeviceProfile::reload_device_profiles ()
{
	vector<string> s;
	vector<string> devprofiles;
	Searchpath spath (devprofile_search_path());

	find_files_matching_filter (devprofiles, spath, devprofile_filter, 0, false, true);
	device_profiles.clear ();

	if (devprofiles.empty()) {
		error << "No MCP device info files found using " << spath.to_string() << endmsg;
		return;
	}

	for (vector<string>::iterator i = devprofiles.begin(); i != devprofiles.end(); ++i) {
		string fullpath = *i;
		DeviceProfile dp; // has to be initial every loop or info from last added.

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		XMLNode* root = tree.root ();
		if (!root) {
			continue;
		}

		if (dp.set_state (*root, 3000) == 0) { /* version is ignored for now */
			dp.set_path (fullpath);
			device_profiles[dp.name()] = dp;
		}
	}
}

int
DeviceProfile::set_state (const XMLNode& node, int /* version */)
{
	const XMLProperty* prop;
	const XMLNode* child;

	if (node.name() != "MackieDeviceProfile") {
		return -1;
	}

	/* name is mandatory */

	if ((child = node.child ("Name")) == 0 || (prop = child->property ("value")) == 0) {
		return -1;
	} else {
		_name = prop->value();
	}

	if ((child = node.child ("Buttons")) != 0) {
		XMLNodeConstIterator i;
		const XMLNodeList& nlist (child->children());

		for (i = nlist.begin(); i != nlist.end(); ++i) {

			if ((*i)->name() == "Button") {

				if ((prop = (*i)->property ("name")) == 0) {
					error << string_compose ("Button without name in device profile \"%1\" - ignored", _name) << endmsg;
					continue;
				}

				int id = Button::name_to_id (prop->value());
				if (id < 0) {
					error << string_compose ("Unknown button ID \"%1\"", prop->value()) << endmsg;
					continue;
				}

				Button::ID bid = (Button::ID) id;

				ButtonActionMap::iterator b = _button_map.find (bid);

				if (b == _button_map.end()) {
					b = _button_map.insert (_button_map.end(), std::pair<Button::ID,ButtonActions> (bid, ButtonActions()));
				}

				(*i)->get_property ("plain", b->second.plain);
				(*i)->get_property ("control", b->second.control);
				(*i)->get_property ("shift", b->second.shift);
				(*i)->get_property ("option", b->second.option);
				(*i)->get_property ("cmdalt", b->second.cmdalt);
				(*i)->get_property ("shiftcontrol", b->second.shiftcontrol);
			}
		}
	}

	edited = false;

	return 0;
}

XMLNode&
DeviceProfile::get_state () const
{
	XMLNode* node = new XMLNode ("MackieDeviceProfile");
	XMLNode* child = new XMLNode ("Name");

	child->set_property ("value", name());
	node->add_child_nocopy (*child);

	if (_button_map.empty()) {
		return *node;
	}

	XMLNode* buttons = new XMLNode ("Buttons");
	node->add_child_nocopy (*buttons);

	for (ButtonActionMap::const_iterator b = _button_map.begin(); b != _button_map.end(); ++b) {
		XMLNode* n = new XMLNode ("Button");

		n->set_property ("name", Button::id_to_name (b->first));

		if (!b->second.plain.empty()) {
			n->set_property ("plain", b->second.plain);
		}
		if (!b->second.control.empty()) {
			n->set_property ("control", b->second.control);
		}
		if (!b->second.shift.empty()) {
			n->set_property ("shift", b->second.shift);
		}
		if (!b->second.option.empty()) {
			n->set_property ("option", b->second.option);
		}
		if (!b->second.cmdalt.empty()) {
			n->set_property ("cmdalt", b->second.cmdalt);
		}
		if (!b->second.shiftcontrol.empty()) {
			n->set_property ("shiftcontrol", b->second.shiftcontrol);
		}

		buttons->add_child_nocopy (*n);
	}

	return *node;
}

string
DeviceProfile::get_button_action (Button::ID id, int modifier_state) const
{
	ButtonActionMap::const_iterator i = _button_map.find (id);

	if (i == _button_map.end()) {
		return string();
	}

	if (modifier_state == MackieControlProtocol::MODIFIER_CONTROL) {
		return i->second.control;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_SHIFT) {
		return i->second.shift;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_OPTION) {
		return i->second.option;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_CMDALT) {
		return i->second.cmdalt;
	} else if (modifier_state == (MackieControlProtocol::MODIFIER_CONTROL|MackieControlProtocol::MODIFIER_SHIFT)) {
		return i->second.shiftcontrol;
	}

	return i->second.plain;
}

void
DeviceProfile::set_button_action (Button::ID id, int modifier_state, const string& act)
{
	ButtonActionMap::iterator i = _button_map.find (id);

	if (i == _button_map.end()) {
		i = _button_map.insert (std::make_pair (id, ButtonActions())).first;
	}

	string action (act);
	replace_all (action, "<Actions>/", "");

	if (modifier_state == MackieControlProtocol::MODIFIER_CONTROL) {
		i->second.control = action;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_SHIFT) {
		i->second.shift = action;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_OPTION) {
		i->second.option = action;
	} else if (modifier_state == MackieControlProtocol::MODIFIER_CMDALT) {
		i->second.cmdalt = action;
	} else if (modifier_state == (MackieControlProtocol::MODIFIER_CONTROL|MackieControlProtocol::MODIFIER_SHIFT)) {
		i->second.shiftcontrol = action;
	}

	if (modifier_state == 0) {
		i->second.plain = action;
	}

	edited = true;

	save ();
}

string
DeviceProfile::name_when_edited (string const& base)
{
	return string_compose ("%1 %2", base, edited_indicator);
}

string
DeviceProfile::name() const
{
	if (edited) {
		if (_name.find (edited_indicator) == string::npos) {
			/* modify name to included edited indicator */
			return name_when_edited (_name);
		} else {
			/* name already contains edited indicator */
			return _name;
		}
	} else {
		return _name;
	}
}

void
DeviceProfile::set_path (const string& p)
{
	_path = p;
}

/* XXX copied from libs/ardour/utils.cc */

static string
legalize_for_path (const string& str)
{
	string::size_type pos;
	string illegal_chars = "/\\"; /* DOS, POSIX. Yes, we're going to ignore HFS */
	string legal;

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_of (illegal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return string (legal);
}


void
DeviceProfile::save ()
{
	std::string fullpath = user_devprofile_directory();

	if (g_mkdir_with_parents (fullpath.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create user MCP profile folder \"%1\" (%2)"), fullpath, strerror (errno)) << endmsg;
		return;
	}

	fullpath = Glib::build_filename (fullpath, string_compose ("%1%2", legalize_for_path (name()), devprofile_suffix));

	XMLTree tree;
	tree.set_root (&get_state());

	if (!tree.write (fullpath)) {
		error << string_compose ("MCP profile not saved to %1", fullpath) << endmsg;
	}
}
