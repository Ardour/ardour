/*
 * Copyright (C) 2023 Holger Dehnhardt <holger@dehnhardt.org>
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

#include <iostream>
#include <sstream>
#include <vector>

#include <boost/algorithm/string/trim.hpp>

#include "glib-2.0/gio/gio.h"
#include "glib-2.0/glib/gstdio.h"
#include "glibmm-2.4/glibmm/main.h"
#include "glibmm-2.4/glibmm/miscutils.h"
#include "pbd/debug.h"

#include "ardour/filesystem_paths.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "c1_control.h"
#include "console1.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

namespace ArdourSurface {

uint32_t
Console1::load_mappings ()
{
	uint32_t i = 0;
	std::string path = Glib::build_filename (user_config_directory (), "c1mappings");
	GError* error;
	GFile* dir = g_file_new_for_path (path.c_str ());
	if (!g_file_test (path.c_str (), G_FILE_TEST_IS_DIR)) {
		g_file_make_directory (dir, NULL, &error);
	}
	const gchar* fileName;
	GDir* gdir = g_dir_open (path.c_str (), 0, NULL);
	if (gdir == NULL)
		return 0;
	while ((fileName = g_dir_read_name (gdir)) != NULL) {
		// if (!g_str_has_suffix (name, ".remmina"))
		//	continue;
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("Console1::load_mappings - found mapping file: '%1'\n", fileName));

		std::string filePath = Glib::build_filename (path, fileName);
		FILE* fin = g_fopen (filePath.c_str (), "r");
		if (fin) {
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("Console1::load_mappings - opened mapping file: '%1'\n", filePath));
			load_mapping (fin);
			fclose (fin);
		}

		++i;
	}
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::load_mappings - found %1 mapping files\n", i));
	g_dir_close (gdir);
	return i;
}

bool
Console1::load_mapping (FILE* fin)
{
	char tmp[1024];
	PluginMapping pm;
	while (fgets (tmp, 1024, fin) != NULL) {
		istringstream line (tmp);
		std::string token;
		vector<string> strings;
		while (getline (line, token, ';')) {
			boost::algorithm::trim (token);
			strings.push_back (std::move (token));
		}
		if (strings.size () < 2)
			continue;
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("Console1::load_mapping - Name: '%1', Val1: '%2', Val2: '%3' \n",
		                             strings.at (0),
		                             strings.at (1),
		                             strings.size () > 2 ? strings.at (2) : ""));
		if (strings.at (0) == "ID") {
			pm.id = strings.at (1);
		} else if (strings.at (0) == "NAME") {
			pm.name = strings.at (1);
		} else {
			try {
				uint32_t index = std::stoi (strings.at (0));
				// Only store complete mappings: Indey, Name, ControllerId
				if (strings.size () < 3)
					continue;
				PluginParameterMapping parmap;
				parmap.paramIndex = index;
				parmap.name = strings.at (1);
				ControllerMap::const_iterator m = controllerMap.find (strings.at (2));
				if (m == controllerMap.end ())
					continue;
				parmap.controllerId = m->second;
				pm.parameters[index] = std::move (parmap);
			} catch (std::invalid_argument&) {
				continue;
			}
		}
	}
	pluginMappingMap[pm.id] = pm;
	return true;
}

void
Console1::select_plugin (uint32_t plugin_index)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::select_plugin\n");
	if (current_plugin_index == plugin_index) {
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (_current_stripable);
		if (!r) {
			return;
		}
		std::shared_ptr<Processor> proc = r->nth_plugin (plugin_index);
		if (!proc) {
			return;
		}
		if (!proc->display_to_user ()) {
			return;
		}
		std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
		if (!plugin_insert)
			return;
		plugin_insert->ToggleUI ();
		return;
	}
	current_plugin_index = plugin_index;
	map_select_plugin ();
}

void
Console1::map_select_plugin ()
{
	DEBUG_TRACE (DEBUG::Console1, "map_select_plugin())\n");
	bool plugin_availabe = spill_plugins (current_plugin_index);
	for (uint32_t i = 0; i < bank_size; ++i) {
		if (i == current_plugin_index && plugin_availabe) {
			start_blinking (ControllerID (FOCUS1 + i));
		} else if (i != current_strippable_index) {
			stop_blinking (ControllerID (FOCUS1 + i));
		}
	}
}

bool
Console1::spill_plugins (uint32_t plugin_index)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("spill_plugins(%1)\n", plugin_index));
	std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (_current_stripable);
	if (!r) {
		return false;
	}

	plugin_connections.drop_connections ();

	for (auto& e : encoders) {
		e.second->set_plugin_action (0);
		e.second->set_value (0);
	}
	for (auto& c : buttons) {
        if( c.first == ControllerID::TRACK_GROUP )
			continue;
        if( c.first >= ControllerID::FOCUS1 && c.first <= ControllerID::FOCUS20 )
			continue;
		c.second->set_plugin_action (0);
		c.second->set_led_state (false);
	}

	std::shared_ptr<Processor> proc = r->nth_plugin (plugin_index);
	if (!proc) {
		return false;
	}
	if (!proc->display_to_user ()) {
		return false;
	}

#ifdef MIXBUS
	/* don't show channelstrip plugins, use "well known" */
	if (std::dynamic_pointer_cast<PluginInsert> (proc)->is_channelstrip ()) {
		continue;
	}
#endif

	int32_t n_controls = -1;
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Found plugin %1\n", proc->name ()));
	std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
	if (!plugin_insert)
		return false;

	std::shared_ptr<Plugin> plugin = plugin_insert->plugin ();
	if (!plugin)
		return false;

	DEBUG_TRACE (DEBUG::Console1, string_compose ("Found plugin id %1\n", plugin->unique_id ()));

	PluginMappingMap::iterator pmmit = pluginMappingMap.find (plugin->unique_id ());
	if (pmmit == pluginMappingMap.end ()) {
		return true;
	}

	PluginMapping pluginMapping = pmmit->second;
	DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("Plugin mapping found for id %1, name %2\n", pluginMapping.id, pluginMapping.name));

	set<Evoral::Parameter> p = proc->what_can_be_automated ();

	PluginParameterMapping enableMapping = pluginMapping.parameters[-1];
	if (enableMapping.controllerId != 0) {
		try {
			ControllerButton* cb = get_button (enableMapping.controllerId);
			boost::function<void ()> plugin_mapping = [=] () -> void { cb->set_led_state (plugin_insert->enabled ()); };
			cb->set_plugin_action ([=] (uint32_t val) {
				plugin_insert->enable (val == 127);
				DEBUG_TRACE (DEBUG::Console1,
				             string_compose ("ControllerButton Plugin parameter %1: %2 \n", n_controls, val));
			});

			plugin_insert->ActiveChanged.connect (
			  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping), this);
			plugin_insert->ActiveChanged ();
		} catch (ControlNotFoundException&) {
			DEBUG_TRACE (DEBUG::Console1, string_compose ("No ControllerButton found %1\n", n_controls));
		}
	}

	for (set<Evoral::Parameter>::iterator j = p.begin (); j != p.end (); ++j) {
		++n_controls;
		std::string n = proc->describe_parameter (*j);
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Plugin parameter %1: %2\n", n_controls, n));
		if (n == "hidden") {
			continue;
		}
		ParameterDescriptor parameterDescriptor;
		plugin->get_parameter_descriptor (n_controls, parameterDescriptor);
		if (plugin->parameter_is_control (n_controls)) {
			DEBUG_TRACE (DEBUG::Console1, "parameter is control\n");
		}
		if (plugin->parameter_is_output (n_controls)) {
			DEBUG_TRACE (DEBUG::Console1, "parameter is output\n");
		}
		if (plugin->parameter_is_audio (n_controls)) {
			DEBUG_TRACE (DEBUG::Console1, "parameter is audio\n");
		}
		if (plugin->parameter_is_input (n_controls)) {
			std::shared_ptr<AutomationControl> c =
			  plugin_insert->automation_control (Evoral::Parameter (PluginAutomation, 0, n_controls));
			if (c) {
				bool swtch = false;
				if (parameterDescriptor.integer_step && parameterDescriptor.upper == 1) {
					swtch = true;
				}
				PluginParameterMapping ppm = pluginMapping.parameters[n_controls];
				try {
					Encoder* e = get_encoder (ppm.controllerId);
					boost::function<void (bool b, PBD::Controllable::GroupControlDisposition d)> plugin_mapping =
					  [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
						double v = parameterDescriptor.to_interface (c->get_value (), true);
						e->set_value (v * 127);
					};
					e->set_plugin_action ([=] (uint32_t val) {
						double v = val / 127.f;
						c->set_value (parameterDescriptor.from_interface (v, true),
						              PBD::Controllable::GroupControlDisposition::UseGroup);
						DEBUG_TRACE (DEBUG::Console1,
						             string_compose ("Encoder Plugin parameter %1: %2 - %3\n", n_controls, val, v));
					});
					c->Changed.connect (
					  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping, _1, _2), this);
					c->Changed (true, PBD::Controllable::GroupControlDisposition::UseGroup);
					continue;
				} catch (ControlNotFoundException&) {
					DEBUG_TRACE (DEBUG::Console1, string_compose ("No Encoder found %1\n", n_controls));
				}
				try {
					ControllerButton* cb = get_button (ppm.controllerId);
					boost::function<void (bool b, PBD::Controllable::GroupControlDisposition d)> plugin_mapping =
					  [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
						// double v = parameterDescriptor.to_interface (c->get_value (), true);
						// e->set_value (v * 127);
						cb->set_led_state (c->get_value ());
					};
					cb->set_plugin_action ([=] (uint32_t val) {
						double v = val / 127.f;
						c->set_value (parameterDescriptor.from_interface (v, true),
						              PBD::Controllable::GroupControlDisposition::UseGroup);
						DEBUG_TRACE (
						  DEBUG::Console1,
						  string_compose ("ControllerButton Plugin parameter %1: %2 - %3\n", n_controls, val, v));
					});

					c->Changed.connect (
					  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping, _1, _2), this);
					c->Changed (true, PBD::Controllable::GroupControlDisposition::UseGroup);
					continue;
				} catch (ControlNotFoundException&) {
					DEBUG_TRACE (DEBUG::Console1, string_compose ("No ControllerButton found %1\n", n_controls));
				}
			}
		}
	}
	return true;
}

void
Console1::map_p ()
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::map_p");
}
}