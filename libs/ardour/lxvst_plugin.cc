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

#include "ardour/linux_vst_support.h"
#include "ardour/session.h"
#include "ardour/lxvst_plugin.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

LXVSTPlugin::LXVSTPlugin (AudioEngine& e, Session& session, VSTHandle* h)
	: VSTPlugin (e, session, h)
{
	/* Instantiate the plugin and return a VSTState* */

	if (vstfx_instantiate (_handle, Session::vst_callback, this) == 0) {
		throw failed_constructor();
	}

	set_plugin (_state->plugin);
}

LXVSTPlugin::LXVSTPlugin (const LXVSTPlugin &other)
	: VSTPlugin (other)
{
	_handle = other._handle;

	if (vstfx_instantiate (_handle, Session::vst_callback, this) == 0) {
		throw failed_constructor();
	}
	_plugin = _state->plugin;

	// Plugin::setup_controls ();
}

LXVSTPlugin::~LXVSTPlugin ()
{
	vstfx_close (_state);
}

PluginPtr 
LXVSTPluginInfo::load (Session& session)
{
	try {
		PluginPtr plugin;

		if (Config->get_use_lxvst()) {
			VSTHandle* handle;

			handle = vstfx_load(path.c_str());

			if (handle == NULL) {
				error << string_compose(_("LXVST: cannot load module from \"%1\""), path) << endmsg;
			}
			else {
				plugin.reset (new LXVSTPlugin (session.engine(), session, handle));
			}
		}
		else {
			error << _("You asked ardour to not use any LXVST plugins") << endmsg;
			return PluginPtr ((Plugin*) 0);
		}

		plugin->set_info(PluginInfoPtr(new LXVSTPluginInfo(*this)));
		return plugin;
	}

	catch (failed_constructor &err) {
		return PluginPtr ((Plugin*) 0);
	}
}

LXVSTPluginInfo::LXVSTPluginInfo()
{
       type = ARDOUR::LXVST;
}

