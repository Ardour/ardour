/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include "ardour/ardour.h"
#include "ardour/automation_control.h"
#include "ardour/lv2_plugin.h"
#include "ardour/luaproc.h"
#include "ardour/plug_insert_base.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;

bool
PlugInsertBase::parse_plugin_type (XMLNode const& node, PluginType& type, std::string& id) const
{
	std::string str;
	if (!node.get_property ("type", str)) {
		error << _("XML node describing plugin is missing the `type' field") << endmsg;
		return false;
	}

	if (str == X_("ladspa") || str == X_("Ladspa")) { /* handle old school sessions */
		type = ARDOUR::LADSPA;
	} else if (str == X_("lv2")) {
		type = ARDOUR::LV2;
	} else if (str == X_("windows-vst")) {
		type = ARDOUR::Windows_VST;
	} else if (str == X_("lxvst")) {
		type = ARDOUR::LXVST;
	} else if (str == X_("mac-vst")) {
		type = ARDOUR::MacVST;
	} else if (str == X_("audiounit")) {
		type = ARDOUR::AudioUnit;
	} else if (str == X_("luaproc")) {
		type = ARDOUR::Lua;
	} else if (str == X_("vst3")) {
		type = ARDOUR::VST3;
	} else {
		error << string_compose (_("unknown plugin type %1 in plugin insert state"), str) << endmsg;
		return false;
	}

	XMLProperty const* prop = node.property ("unique-id");

	if (prop == 0) {
#ifdef WINDOWS_VST_SUPPORT
		/* older sessions contain VST plugins with only an "id" field.  */
		if (type == ARDOUR::Windows_VST) {
			prop = node.property ("id");
		}
#endif

#ifdef LXVST_SUPPORT
		/*There shouldn't be any older sessions with linuxVST support.. but anyway..*/
		if (type == ARDOUR::LXVST) {
			prop = node.property ("id");
		}
#endif

		/* recheck  */
		if (prop == 0) {
			error << _("Plugin has no unique ID field") << endmsg;
			return false;
		}
	}

	id = prop->value();
	return true;
}

boost::shared_ptr<Plugin>
PlugInsertBase::find_and_load_plugin (Session& s, XMLNode const& node, PluginType& type, std::string const& unique_id, bool& any_vst)
{
	/* Find and load plugin module */
	boost::shared_ptr<Plugin> plugin = find_plugin (s, unique_id, type);

	/* treat VST plugins equivalent if they have the same uniqueID
	 * allow to move sessions windows <> linux */
#ifdef LXVST_SUPPORT
	if (plugin == 0 && (type == ARDOUR::Windows_VST || type == ARDOUR::MacVST)) {
		type = ARDOUR::LXVST;
		plugin = find_plugin (s, unique_id, type);
		if (plugin) {
			any_vst = true;
		}
	}
#endif

#ifdef WINDOWS_VST_SUPPORT
	if (plugin == 0 && (type == ARDOUR::LXVST || type == ARDOUR::MacVST)) {
		type = ARDOUR::Windows_VST;
		plugin = find_plugin (s, unique_id, type);
		if (plugin) { any_vst = true; }
	}
#endif

#ifdef MACVST_SUPPORT
	if (plugin == 0 && (type == ARDOUR::Windows_VST || type == ARDOUR::LXVST)) {
		type = ARDOUR::MacVST;
		plugin = find_plugin (s, unique_id, type);
		if (plugin) { any_vst = true; }
	}
#endif

	if (plugin == 0 && type == ARDOUR::Lua) {
		/* unique ID (sha1 of script) was not found,
		 * load the plugin from the serialized version in the
		 * session-file instead.
		 */
		boost::shared_ptr<LuaProc> lp (new LuaProc (s.engine(), s, ""));
		XMLNode *ls = node.child (lp->state_node_name().c_str());
		if (ls && lp) {
			if (0 == lp->set_script_from_state (*ls)) {
				plugin = lp;
			}
		}
	}

	if (plugin == 0) {
		error << string_compose(
				_("Found a reference to a plugin (\"%1\") that is unknown.\n"
					"Perhaps it was removed or moved since it was last used."),
				unique_id)
			<< endmsg;
		return boost::shared_ptr<Plugin> ();
	}
	return plugin;
}

void
PlugInsertBase::set_control_ids (const XMLNode& node, int version)
{
	const XMLNodeList& nlist = node.children();
	for (XMLNodeConstIterator iter = nlist.begin(); iter != nlist.end(); ++iter) {
		if ((*iter)->name() != Controllable::xml_node_name) {
			continue;
		}

		uint32_t p = (uint32_t)-1;
		std::string str;
		if ((*iter)->get_property (X_("symbol"), str)) {
			boost::shared_ptr<LV2Plugin> lv2plugin = boost::dynamic_pointer_cast<LV2Plugin> (plugin ());
			if (lv2plugin) {
				p = lv2plugin->port_index(str.c_str());
			}
		}
		if (p == (uint32_t)-1) {
			(*iter)->get_property (X_("parameter"), p);
		}

		if (p == (uint32_t)-1) {
			continue;
		}

		/* this may create the new controllable */
		boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, p));

		if (!c) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c);
		if (ac) {
			ac->set_state (**iter, version);
		}
	}
}

void
PlugInsertBase::preset_load_set_value (uint32_t p, float v)
{
	boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (Evoral::ControlSet::control (Evoral::Parameter(PluginAutomation, 0, p), false));
	if (!ac) {
		return;
	}

	if (ac->automation_state() & Play) {
		return;
	}

	ac->start_touch (timepos_t (ac->session ().audible_sample()));
	ac->set_value (v, Controllable::NoGroup);
	ac->stop_touch (timepos_t (ac->session ().audible_sample()));
}
