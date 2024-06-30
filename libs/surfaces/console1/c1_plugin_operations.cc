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

#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include "glib-2.0/gio/gio.h"
#include "glib-2.0/glib/gstdio.h"
#include "glibmm-2.4/glibmm/main.h"
#include "glibmm-2.4/glibmm/miscutils.h"
#include "pbd/debug.h"
#include "pbd/i18n.h"

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

bool
Console1::ensure_config_dir ()
{
	std::string path = Glib::build_filename (user_config_directory (), config_dir_name);
	GError* error = 0;
	GFile* dir = g_file_new_for_path (path.c_str ());
	if (!g_file_test (path.c_str (), G_FILE_TEST_IS_DIR)) {
		g_file_make_directory (dir, NULL, &error);
	}
	return error == 0 || error->code == 0;
}

uint32_t
Console1::load_mappings ()
{
    if( mappings_loaded )
		return pluginMappingMap.size ();
        
	uint32_t i = 0;
	if (!ensure_config_dir ())
		return 1;

	std::string path = Glib::build_filename (user_config_directory (), config_dir_name);
	const gchar* file_name;
	GDir* gdir = g_dir_open (path.c_str (), 0, NULL);
	if (gdir == NULL)
		return 0;
	while ((file_name = g_dir_read_name (gdir)) != NULL) {
		if (!g_str_has_suffix (file_name, ".xml"))
			continue;
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("Console1::load_mappings - found mapping file: '%1'\n", file_name));

		std::string file_path = Glib::build_filename (path, file_name);
		XMLTree tree;
		XMLNode* mapping_xml = 0;
		if (tree.read (file_path)) {
			mapping_xml = new XMLNode (*(tree.root ()));
		} else {
			warning << string_compose (_ ("Could not understand XML file %1"), file_path) << endmsg;
		}

		if (mapping_xml) {
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("Console1::load_mappings - opened mapping file: '%1'\n", file_path));
			load_mapping (mapping_xml);
		}
		++i;
	}
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::load_mappings - found %1 mapping files\n", i));
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::load_mappings - loaded %1 mapping files\n", pluginMappingMap.size()));
	g_dir_close (gdir);
	mappings_loaded = true;
	return i;
}

bool
Console1::load_mapping (XMLNode* mapping_xml)
{
	// char tmp[1024];
	PluginMapping pm;
	const XMLNodeList& nlist = mapping_xml->children ();

	mapping_xml->get_property ("ID", pm.id);
	mapping_xml->get_property ("NAME", pm.name);

	XMLNodeConstIterator i;
	for (i = nlist.begin (); i != nlist.end (); ++i) {
		std::string param_id;
		std::string param_type;
		std::string param_name;
		std::string param_mapping;

		(*i)->get_property ("id", param_id);
		uint32_t index = std::stoi (param_id);

		(*i)->get_property ("type", param_type);

		const XMLNodeList& plist = (*i)->children ();

		XMLNodeConstIterator j;
		PluginParameterMapping parmap;
		for (j = plist.begin (); j != plist.end (); ++j) {
			if ((*j)->name () == "name") {
				param_name = (*j)->child_content ();
			} else if ((*j)->name () == "mapping") {
				param_mapping = (*j)->child_content ();
				(*j)->get_property ("shift", parmap.shift);
			}
		}
		parmap.paramIndex = index;
		parmap.name = param_name;
   		parmap.is_switch = (param_type == "switch");
		if (!param_mapping.empty ()) {
			ControllerMap::const_iterator m = controllerMap.find (param_mapping);
			if (m != controllerMap.end ())
            {
    			parmap.controllerId = m->second;
            }
		}
        else{
			pm.configured = false;
   			parmap.controllerId = CONTROLLER_NONE;
		}
		pm.parameters[index] = std::move (parmap);
	}
	pluginMappingMap[pm.id] = pm;
	return true;
}

void
Console1::create_mapping (const std::shared_ptr<Processor> proc, const std::shared_ptr<Plugin> plugin)
{
	XMLTree* tree = new XMLTree ();
	XMLNode node = XMLNode ("c1plugin-mapping");
	node.set_property ("ID", plugin->unique_id ());
	node.set_property ("NAME", plugin->name ());
	int32_t n_controls = -1;

	set<Evoral::Parameter> p = proc->what_can_be_automated ();
	for (set<Evoral::Parameter>::iterator j = p.begin (); j != p.end (); ++j) {
		++n_controls;
		std::string n = proc->describe_parameter (*j);
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Plugin parameter %1: %2\n", n_controls, n));
		if (n == "hidden") {
			continue;
		}
		XMLNode param = XMLNode ("param-mapping");
		param.set_property ("id", n_controls);
		XMLNode name = XMLNode ("name");
		XMLNode c = XMLNode ("c", plugin->parameter_label (n_controls).c_str ());
		name.add_child_copy (c);
		XMLNode mapping = XMLNode ("mapping");
		mapping.set_property ("shift", "false");
		param.add_child_copy (name);
		param.add_child_copy (mapping);
		node.add_child_copy (param);
	}

	tree->set_root (&node);

	if (!ensure_config_dir ())
		return;

	std::string filename = Glib::build_filename (
	  user_config_directory (), config_dir_name, string_compose ("%1.%2", plugin->unique_id (), "xml"));

	tree->set_filename (filename);
	tree->write ();
}

void
Console1::write_plugin_mapping (PluginMapping &mapping)
{
    DEBUG_TRACE (DEBUG::Console1, "write_plugin_mapping \n");
	XMLTree* tree = new XMLTree ();
	XMLNode node = XMLNode ("c1plugin-mapping");
	node.set_property ("ID", mapping.id);
	node.set_property ("NAME", mapping.name);

	for (const auto& plugin_param : mapping.parameters ) {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Plugin parameter %1: %2\n",plugin_param.first ,plugin_param.second.name));
		XMLNode param = XMLNode ("param-mapping");
		param.set_property ("id", plugin_param.second.paramIndex);
		XMLNode name = XMLNode ("name");
		XMLNode c = XMLNode ("c", plugin_param.second.name );
		name.add_child_copy (c);
		XMLNode mapping = XMLNode ("mapping");
		mapping.set_property ("shift", plugin_param.second.shift);
        XMLNode controller = XMLNode ("c", findControllerNameById(plugin_param.second.controllerId) );
		mapping.add_child_copy (controller);
		param.add_child_copy (name);
		param.add_child_copy (mapping);
		node.add_child_copy (param);
	}

	tree->set_root (&node);

	if (!ensure_config_dir ())
		return;

	std::string filename = Glib::build_filename (
	  user_config_directory (), config_dir_name, string_compose ("%1.%2", mapping.id, "xml"));

	tree->set_filename (filename);
	tree->write ();
	load_mapping (&node);
}

bool
Console1::select_plugin (const int32_t plugin_index)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::select_plugin\n");
	if (current_plugin_index == plugin_index) {
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (_current_stripable);
		if (!r) {
			return false;
		}
#ifdef MIXBUS
		std::shared_ptr<Processor> proc = r->nth_plugin (selected_intern_plugin_index);
#else
		std::shared_ptr<Processor> proc = r->nth_plugin (plugin_index);
#endif
		if (!proc) {
			return false;
		}
		if (!proc->display_to_user ()) {
			return false;
		}
		std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
		if (!plugin_insert)
			return false;
		plugin_insert->ToggleUI ();
		return true;
	} else if (map_select_plugin (plugin_index)) {
		return true;
	}
	return false;
}

bool
Console1::map_select_plugin (const int32_t plugin_index)
{
	DEBUG_TRACE (DEBUG::Console1, "map_select_plugin())\n");
	if (spill_plugins (plugin_index)) {
		for (uint32_t i = 0; i < bank_size; ++i) {
			if ((int)i == plugin_index) {
				start_blinking (ControllerID (FOCUS1 + i));
			} else if (i != current_strippable_index) {
				stop_blinking (ControllerID (FOCUS1 + i));
			}
		}
		current_plugin_index = plugin_index;
		return true;
	} else {
		get_button (ControllerID (FOCUS1 + plugin_index))
		  ->set_led_state (plugin_index == (int)current_strippable_index);
	}
	return false;
}

void
Console1::remove_plugin_operations ()
{
	plugin_connections.drop_connections ();

	for (auto& e : encoders) {
		e.second->set_plugin_action (0);
		e.second->set_plugin_shift_action (0);
		e.second->set_value (0);
	}
	for (auto& b : buttons) {
		if (b.first == ControllerID::TRACK_GROUP)
			continue;
		if (b.first >= ControllerID::FOCUS1 && b.first <= ControllerID::FOCUS20)
			continue;
		b.second->set_plugin_action (0);
		b.second->set_plugin_shift_action (0);
		b.second->set_led_state (false);
	}
	for (auto& m : multi_buttons) {
		m.second->set_plugin_action (0);
		m.second->set_plugin_shift_action (0);
		m.second->set_led_state (false);
	}
}

std::shared_ptr<Processor>
Console1::find_plugin (const int32_t plugin_index)
{
	int32_t int_plugin_index = -1;
	int32_t ext_plugin_index = -1;
	std::shared_ptr<Processor> proc;
	DEBUG_TRACE (DEBUG::Console1, string_compose ("find_plugin(%1)\n", plugin_index));
	std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (_current_stripable);
	if (!r) {
		return proc;
	}
	remove_plugin_operations ();

	while ((ext_plugin_index < plugin_index) && (int_plugin_index < (int)bank_size)) {
		++int_plugin_index;

		proc = r->nth_plugin (int_plugin_index);
		if (!proc) {
			continue;
			;
		}
		if (!proc->display_to_user ()) {
			continue;
		}

#ifdef MIXBUS
		/* don't show channelstrip plugins */
		if (std::dynamic_pointer_cast<PluginInsert> (proc)->is_channelstrip ()) {
			continue;
		}
#endif
		++ext_plugin_index;
	}

#ifdef MIXBUS
	selected_intern_plugin_index = int_plugin_index;
#endif
	return proc;
}

bool
Console1::spill_plugins (const int32_t plugin_index)
{
	bool mapping_found = false;

	remove_plugin_operations ();

	std::shared_ptr<Processor> proc = find_plugin (plugin_index);
	if (!proc)
		return false;

	int32_t n_controls = -1;
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Found plugin %1\n", proc->name ()));
	std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
	if (!plugin_insert)
		return false;

	std::shared_ptr<Plugin> plugin = plugin_insert->plugin ();
	if (!plugin)
		return false;

	DEBUG_TRACE (DEBUG::Console1, string_compose ("Found plugin id %1\n", plugin->unique_id ()));

	try {
		ControllerButton* cb = get_button (ControllerID::MUTE);
		boost::function<void ()> plugin_mapping = [=] () -> void { cb->set_led_state (!plugin_insert->enabled ()); };
		cb->set_plugin_action ([=] (uint32_t val) {
			plugin_insert->enable (val == 0);
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("ControllerButton Plugin parameter %1: %2 \n", n_controls, val));
		});

		plugin_insert->ActiveChanged.connect (
		  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping), this);
		plugin_insert->ActiveChanged ();
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("No ControllerButton found %1\n", n_controls));
	}
	PluginMappingMap::iterator pmmit = pluginMappingMap.find (plugin->unique_id ());
	mapping_found = (pmmit != pluginMappingMap.end ());

	if (!mapping_found) {
		if (create_mapping_stubs) {
			create_mapping (proc, plugin);
		}
		return true;
	}

	PluginMapping pluginMapping = pmmit->second;

	DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("Plugin mapping found for id %1, name %2\n", pluginMapping.id, pluginMapping.name));

	set<Evoral::Parameter> p = proc->what_can_be_automated ();

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
				PluginParameterMapping ppm = pluginMapping.parameters[n_controls];
				bool swtch = false;
				if (parameterDescriptor.integer_step && parameterDescriptor.upper == 1) {
					swtch = true;
				} else if (ppm.is_switch) {
					swtch = true;
				}
				if (!swtch) {
					try {
						Encoder* e = get_encoder (ppm.controllerId);
						boost::function<void (bool b, PBD::Controllable::GroupControlDisposition d)> plugin_mapping =
						  [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
							double v = parameterDescriptor.to_interface (c->get_value (), true);
							e->set_value (v * 127);
							DEBUG_TRACE (
							  DEBUG::Console1,
							  string_compose ("<-Encoder Plugin parameter %1: %2 - %3\n", n_controls, v * 127, v));
						};
						e->set_plugin_action ([=] (uint32_t val) {
							double v = val / 127.f;
							c->set_value (parameterDescriptor.from_interface (v, true),
							              PBD::Controllable::GroupControlDisposition::UseGroup);
							DEBUG_TRACE (
							  DEBUG::Console1,
							  string_compose ("->Encoder Plugin parameter %1: %2 - %3\n", n_controls, val, v));
						});
						c->Changed.connect (
						  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping, _1, _2), this);
						c->Changed (true, PBD::Controllable::GroupControlDisposition::UseGroup);
						continue;
					} catch (ControlNotFoundException const&) {
						DEBUG_TRACE (DEBUG::Console1, string_compose ("No Encoder found %1\n", n_controls));
					}
				} else {
					try {
						ControllerButton* cb = get_button (ppm.controllerId);
						boost::function<void (bool b, PBD::Controllable::GroupControlDisposition d)> plugin_mapping =
						  [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
							cb->set_led_state (c->get_value ());
							DEBUG_TRACE (DEBUG::Console1,
							             string_compose ("<-ControllerButton Plugin parameter %1: %2 \n",
							                             n_controls,
							                             c->get_value ()));
						};
						cb->set_plugin_action ([=] (uint32_t val) {
							double v = val / 127.f;
							c->set_value (parameterDescriptor.from_interface (v, true),
							              PBD::Controllable::GroupControlDisposition::UseGroup);
							DEBUG_TRACE (
							  DEBUG::Console1,
							  string_compose ("->ControllerButton Plugin parameter %1: %2 - %3\n", n_controls, val, v));
						});

						c->Changed.connect (
						  plugin_connections, MISSING_INVALIDATOR, boost::bind (plugin_mapping, _1, _2), this);
						c->Changed (true, PBD::Controllable::GroupControlDisposition::UseGroup);
						continue;
					} catch (ControlNotFoundException const&) {
						DEBUG_TRACE (DEBUG::Console1, string_compose ("No ControllerButton found %1\n", n_controls));
					}
				}
			}
		}
	}
	return true;
}

Glib::RefPtr<Gtk::ListStore> Console1::getPluginControllerModel()
{
    plugin_controller_model = Gtk::ListStore::create (plugin_controller_columns);
	Gtk::TreeModel::Row plugin_controller_combo_row;
    for( const auto &controller : controllerMap ){
        plugin_controller_combo_row = *(plugin_controller_model->append ());
		plugin_controller_combo_row[plugin_controller_columns.controllerId] = controller.second;
		plugin_controller_combo_row[plugin_controller_columns.controllerName] = X_(controller.first);
	}
	return plugin_controller_model;
}

}
