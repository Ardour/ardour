/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/filesystem_paths.h"
#include "ardour/mac_vst_plugin.h"
#include "ardour/mac_vst_support.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MacVSTPlugin::MacVSTPlugin (AudioEngine& e, Session& session, VSTHandle* h, int unique_id)
	: VSTPlugin (e, session, h)
{
	/* Instantiate the plugin and return a VSTState* */

	Session::vst_current_loading_id = unique_id;
	if ((_state = mac_vst_instantiate (_handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor ();
	}
	open_plugin ();
	Session::vst_current_loading_id = 0;

	init_plugin ();
}

MacVSTPlugin::MacVSTPlugin (const MacVSTPlugin &other)
	: VSTPlugin (other)
{
	_handle = other._handle;

	Session::vst_current_loading_id = PBD::atoi (other.unique_id ());
	if ((_state = mac_vst_instantiate (_handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor ();
	}
	open_plugin ();
	Session::vst_current_loading_id = 0;

	XMLNode* root = new XMLNode (other.state_node_name ());
	other.add_state (root);
	set_state (*root, Stateful::loading_state_version);
	delete root;

	init_plugin ();
}

MacVSTPlugin::~MacVSTPlugin ()
{
	mac_vst_close (_state);
}

void
MacVSTPlugin::open_plugin ()
{
	VSTPlugin::open_plugin ();
	_plugin->dispatcher (_plugin, effCanDo, 0, 0, const_cast<char*> ("hasCockosViewAsConfig"), 0.0f);
}

PluginPtr
MacVSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_macvst ()) {
			VSTHandle* handle;

			handle = mac_vst_load (path.c_str ());

			if (handle == NULL) {
				error << string_compose (_("MacVST: cannot load module from \"%1\""), path) << endmsg;
				return PluginPtr ((Plugin*) 0);
			} else {
				plugin.reset (new MacVSTPlugin (session.engine (), session, handle, PBD::atoi (unique_id)));
			}
		} else {
			error << _("You asked ardour to not use any MacVST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info (PluginInfoPtr (new MacVSTPluginInfo (*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

std::vector<Plugin::PresetRecord>
MacVSTPluginInfo::get_presets (bool user_only) const
{
	std::vector<Plugin::PresetRecord> p;

	if (!Config->get_use_macvst ()) {
		return p;
	}

	if (!user_only) {
		// TODO - cache, instantiating the plugin can be heavy
		/* Built-in presets */
		VSTHandle* handle = mac_vst_load (path.c_str ());
		Session::vst_current_loading_id = atoi (unique_id);
		AEffect* plugin = handle->main_entry (Session::vst_callback);
		Session::vst_current_loading_id = 0;
		plugin->ptr1 = NULL;

		plugin->dispatcher (plugin, effOpen, 0, 0, 0, 0); // :(
		int const vst_version = plugin->dispatcher (plugin, effGetVstVersion, 0, 0, NULL, 0);

		for (int i = 0; i < plugin->numPrograms; ++i) {
			Plugin::PresetRecord r (string_compose (X_("VST:%1:%2"), unique_id, std::setw(4), std::setfill('0'), i), "", false);
			if (vst_version >= 2) {
				char buf[256];
				if (plugin->dispatcher (plugin, 29, i, 0, buf, 0) == 1) {
					r.label = buf;
				} else {
					r.label = string_compose (_("Preset %1"), i);
				}
			} else {
				r.label = string_compose (_("Preset %1"), i);
			}
			p.push_back (r);
		}

		plugin->dispatcher (plugin, effMainsChanged, 0, 0, 0, 0);
		plugin->dispatcher (plugin, effClose, 0, 0, 0, 0); // :(

		if (handle->plugincnt) {
			handle->plugincnt--;
		}
		mac_vst_unload (handle);
	}

	/* user presets */
	XMLTree* t = new XMLTree;
	std::string pf = Glib::build_filename (ARDOUR::user_config_directory (), "presets", string_compose ("vst-%1", unique_id));
	if (Glib::file_test (pf, Glib::FILE_TEST_EXISTS)) {
		t->set_filename (pf);
		if (t->read ()) { // TODO read names only. skip parsing the actual data
			XMLNode* root = t->root ();
			for (XMLNodeList::const_iterator i = root->children ().begin (); i != root->children ().end (); ++i) {
				XMLProperty const * uri = (*i)->property (X_("uri"));
				XMLProperty const * label = (*i)->property (X_("label"));
				p.push_back (Plugin::PresetRecord (uri->value (), label->value (), true));
			}
		}
	}
	delete t;
	return p;
}

MacVSTPluginInfo::MacVSTPluginInfo (VST2Info const& nfo) : VSTPluginInfo (nfo)
{
	type = ARDOUR::MacVST;
}
