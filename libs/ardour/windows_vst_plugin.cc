/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "fst.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/filesystem_paths.h"
#include "ardour/windows_vst_plugin.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

WindowsVSTPlugin::WindowsVSTPlugin (AudioEngine& e, Session& session, VSTHandle* h, int unique_id)
	: VSTPlugin (e, session, h)
{
	Session::vst_current_loading_id = unique_id;
	if ((_state = fst_instantiate (_handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	open_plugin ();
	Session::vst_current_loading_id = 0;

	init_plugin ();
}

WindowsVSTPlugin::WindowsVSTPlugin (const WindowsVSTPlugin &other)
	: VSTPlugin (other)
{
	_handle = other._handle;

	Session::vst_current_loading_id = PBD::atoi(other.unique_id());
	if ((_state = fst_instantiate (_handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	open_plugin ();
	Session::vst_current_loading_id = 0;

	XMLNode* root = new XMLNode (other.state_node_name ());
	other.add_state (root);
	set_state (*root, Stateful::loading_state_version);
	delete root;

	init_plugin ();
}

WindowsVSTPlugin::~WindowsVSTPlugin ()
{
	deactivate ();
	fst_close (_state);
}

PluginPtr
WindowsVSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_windows_vst ()) {
			VSTHandle* handle;

			handle = fst_load(path.c_str());

			if (!handle) {
				error << string_compose(_("VST: cannot load module from \"%1\""), path) << endmsg;
				return PluginPtr ((Plugin*) 0);
			} else {
				plugin.reset (new WindowsVSTPlugin (session.engine(), session, handle, PBD::atoi(unique_id)));
			}
		} else {
			error << _("You asked ardour to not use any VST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info(PluginInfoPtr(new WindowsVSTPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

std::vector<Plugin::PresetRecord>
WindowsVSTPluginInfo::get_presets (bool user_only) const
{
	std::vector<Plugin::PresetRecord> p;

	if (!Config->get_use_lxvst()) {
		return p;
	}

	if (!user_only) {
		// TODO cache and load factory-preset names
	}

	/* user presets */
	XMLTree* t = new XMLTree;
	std::string pf = Glib::build_filename (ARDOUR::user_config_directory (), "presets", string_compose ("vst-%1", unique_id));
	if (Glib::file_test (pf, Glib::FILE_TEST_EXISTS)) {
		t->set_filename (pf);
		if (t->read ()) {
			XMLNode* root = t->root ();
			for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
				XMLProperty const * uri = (*i)->property (X_("uri"));
				XMLProperty const * label = (*i)->property (X_("label"));
				p.push_back (Plugin::PresetRecord (uri->value(), label->value(), true));
			}
		}
	}
	delete t;
	return p;
}

WindowsVSTPluginInfo::WindowsVSTPluginInfo (VST2Info const& nfo) : VSTPluginInfo (nfo)
{
	type = ARDOUR::Windows_VST;
}

