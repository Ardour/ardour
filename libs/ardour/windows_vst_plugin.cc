/*
    Copyright (C) 2004 Paul Davis

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

#include "fst.h"

#include "ardour/windows_vst_plugin.h"
#include "ardour/session.h"

#include "i18n.h"

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
	Session::vst_current_loading_id = 0;

	set_plugin (_state->plugin);
}

WindowsVSTPlugin::WindowsVSTPlugin (const WindowsVSTPlugin &other)
	: VSTPlugin (other)
{
	_handle = other._handle;

	Session::vst_current_loading_id = PBD::atoi(other.unique_id());
	if ((_state = fst_instantiate (_handle, Session::vst_callback, this)) == 0) {
		throw failed_constructor();
	}
	Session::vst_current_loading_id = 0;
	
	_plugin = _state->plugin;
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

WindowsVSTPluginInfo::WindowsVSTPluginInfo()
{
       type = ARDOUR::Windows_VST;
}

