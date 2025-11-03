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
using namespace PBD;
using namespace Glib;
using namespace std;

namespace Console1
{

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
		return plugin_mapping_map.size ();
        
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
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::load_mappings - loaded %1 mapping files\n", plugin_mapping_map.size()));
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
				(*j)->get_property ("is_switch", parmap.is_switch);
			}
		}
		parmap.paramIndex = index;
		parmap.name = param_name;
		if (!param_mapping.empty ()) {
			ControllerNameIdMap::const_iterator m = controllerNameIdMap.find (param_mapping);
			if (m != controllerNameIdMap.end ())
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
	plugin_mapping_map[pm.id] = pm;
	return true;
}

void
Console1::create_plugin_mapping_stubs (const std::shared_ptr<Processor> proc, const std::shared_ptr<Plugin> plugin)
{
    DEBUG_TRACE (DEBUG::Console1, "create_plugin_mapping_stubs \n");
	XMLTree* tree = new XMLTree ();
	XMLNode node = XMLNode ("c1plugin-mapping");
    if( plugin->unique_id() == "" )
	    return;
    node.set_property ("ID", plugin->unique_id ());
    node.set_property ("NAME", plugin->name ());
    int32_t n_controls = -1;

    set<Evoral::Parameter> p = proc->what_can_be_automated ();
    for (set<Evoral::Parameter>::iterator j = p.begin (); j != p.end (); ++j) {
	    ++n_controls;
	    std::string n = proc->describe_parameter (*j);
	    DEBUG_TRACE (DEBUG::Console1, string_compose ("create_plugin_mapping_stubs: Plugin parameter %1: %2\n", n_controls, n));
	    if (n == "hidden") {
		    continue;
	    }
	    ParameterDescriptor parameterDescriptor;
	    plugin->get_parameter_descriptor (n_controls, parameterDescriptor);
	    XMLNode param = XMLNode ("param-mapping");
	    param.set_property ("id", n_controls);
	    XMLNode name = XMLNode ("name");
	    XMLNode c    = XMLNode ("c", plugin->parameter_label (n_controls).c_str ());
	    name.add_child_copy (c);
	    XMLNode mapping = XMLNode ("mapping");
	    mapping.set_property ("shift", "false");
	    mapping.set_property ("is_switch", parameterDescriptor.toggled ? 1 : 0);
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
	load_mapping (&node);
	PluginStubAdded ();
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
		DEBUG_TRACE (DEBUG::Console1, string_compose ("write_plugin_mapping: Plugin parameter %1: %2 - shift: %3\n", plugin_param.first, plugin_param.second.name, plugin_param.second.shift));
		XMLNode param = XMLNode ("param-mapping");
		param.set_property ("id", plugin_param.second.paramIndex);
		XMLNode name = XMLNode ("name");
		XMLNode c = XMLNode ("c", plugin_param.second.name );
		name.add_child_copy (c);
		XMLNode mapping = XMLNode ("mapping");
		mapping.set_property ("shift", plugin_param.second.shift);
		mapping.set_property ("is_switch", plugin_param.second.is_switch);
		XMLNode controller = XMLNode ("c", findControllerNameById (plugin_param.second.controllerId));
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
	midi_assign_mode = false;
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

	for (auto& c : controllerMap) {
		if (c.first == ControllerID::TRACK_GROUP)
			continue;
		if (c.first >= ControllerID::FOCUS1 && c.first <= ControllerID::FOCUS20)
			continue;
		c.second->set_plugin_action (0);
		c.second->set_plugin_shift_action (0);
		c.second->clear_value ();
        if( c.second->get_type() == ControllerType::CONTROLLER_BUTTON && c.first != ControllerID::PRESET )
        {
	    	ControllerButton* b = dynamic_cast<ControllerButton *> (c.second);
            b->set_led_state (false);
        } else if (c.second->get_type () == ControllerType::MULTISTATE_BUTTON )
        {
            MultiStateButton* b = dynamic_cast<MultiStateButton *> (c.second);
            b->set_led_state (false);
        }
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

	while ((ext_plugin_index < plugin_index) && (int_plugin_index < (int)bank_size)) {
		++int_plugin_index;

		DEBUG_TRACE (DEBUG::Console1, string_compose ("find_plugin: int index %1, ext index %2\n", int_plugin_index, ext_plugin_index));
		proc = r->nth_plugin (int_plugin_index);
		if (!proc) {
			DEBUG_TRACE (DEBUG::Console1, "find_plugin: plugin not found\n");
			continue;
		}
		DEBUG_TRACE (DEBUG::Console1, "find_plugin: plugin found\n");
		if (!proc->display_to_user ()) {
			DEBUG_TRACE (DEBUG::Console1, "find_plugin: display to user failed\n");
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
Console1::setup_plugin_mute_button(const std::shared_ptr<PluginInsert>& plugin_insert)
{
    int32_t n_controls = -1;
    try {
        ControllerButton* cb = get_button (ControllerID::MUTE);
        std::function<void ()> plugin_mapping = [=] () -> void { cb->set_led_state (!plugin_insert->enabled ()); };
        cb->set_plugin_action ([=] (uint32_t val) {
            plugin_insert->enable (val == 0);
            DEBUG_TRACE (DEBUG::Console1,
                         string_compose ("ControllerButton Plugin parameter %1: %2 \n", n_controls, val));
        });

        plugin_insert->ActiveChanged.connect (
          plugin_connections, MISSING_INVALIDATOR, std::bind (plugin_mapping), this);
        plugin_insert->ActiveChanged ();
        return true;
    } catch (ControlNotFoundException const&) {
        DEBUG_TRACE (DEBUG::Console1, string_compose ("No ControllerButton found %1\n", n_controls));
        return false;
    }
}

bool
Console1::setup_plugin_controller (const PluginParameterMapping& ppm, int32_t n_controls,
                                   const ParameterDescriptor&                parameterDescriptor,
                                   const std::shared_ptr<AutomationControl>& ac)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::setup_plugin_controller");
	try {
		Controller* controller = get_controller (ppm.controllerId);
		if (!ppm.shift)
			controller->set_plugin_action ([=] (uint32_t val) {
				double v          = val / 127.f;
				double translated = parameterDescriptor.from_interface (v, true);
				ac->set_value (translated,
				               PBD::Controllable::GroupControlDisposition::UseGroup);
				DEBUG_TRACE (
				    DEBUG::Console1,
				    string_compose ("from: ->Encoder Plugin parameter %1: origin %2 calculated %3 translated %4\n", n_controls, val, v, translated));
			});
		else
			controller->set_plugin_shift_action ([=] (uint32_t val) {
				double v          = val / 127.f;
				double translated = parameterDescriptor.from_interface (v, true);
				ac->set_value (translated,
				               PBD::Controllable::GroupControlDisposition::UseGroup);
				DEBUG_TRACE (
				    DEBUG::Console1,
				    string_compose ("from: ->Encoder Plugin shift-parameter %1: origin %2 calculated %3 translated %4\n", n_controls, val, v, translated));
			});
		return set_plugin_receive_connection (controller, ac, parameterDescriptor, ppm);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("No Encoder found %1\n", n_controls));
		return false;
	}
}

bool
Console1::set_plugin_receive_connection (Controller* controller, const std::shared_ptr<AutomationControl>& ac, const ParameterDescriptor& parameterDescriptor, const PluginParameterMapping& ppm)
{
	DEBUG_TRACE (DEBUG::Console1, "Console1::set_plugin_receive_connection \n");

	if (ppm.shift != shift_state)
		return false;

	std::function<void (bool b, PBD::Controllable::GroupControlDisposition d)> plugin_mapping;

	switch (controller->get_type ()) {
		case ControllerType::ENCODER: {
			Encoder* e = dynamic_cast<Encoder*> (controller);
			if (e) {
				DEBUG_TRACE (DEBUG::Console1, "Console1::set_plugin_receive_connection ENCODER\n");

				plugin_mapping =
				    [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
					double origin = ac->get_value ();
					double v      = parameterDescriptor.to_interface (origin, true);
					e->set_value (v * 127);
					DEBUG_TRACE (
					    DEBUG::Console1,
					    string_compose ("to: <-Encoder Plugin parameter %1: origin %2 translated %3 - %4\n", ppm.paramIndex, origin, v, v * 127));
				};
				DEBUG_TRACE (DEBUG::Console1, string_compose ("ENCODER has plugin_action %1, has shitft_plugin_action %2\n", e->get_plugin_action () ? "Yes" : "No", e->get_plugin_shift_action () ? "Yes" : "No"));
			}
		};
		    break;
		case ControllerType::CONTROLLER_BUTTON: {
			ControllerButton* button = dynamic_cast<ControllerButton*> (controller);
			if (button) {
				DEBUG_TRACE (DEBUG::Console1, "Console1::set_plugin_receive_connection CONTROLLER_BUTTON \n");

				plugin_mapping = [=] (bool b, PBD::Controllable::GroupControlDisposition d) -> void {
					button->set_led_state (ac->get_value ());
					DEBUG_TRACE (DEBUG::Console1,
					             string_compose ("<-ControllerButton Plugin parameter %1: %2 \n",
					                             ppm.paramIndex,
					                             ac->get_value ()));
				};
			}
		};
		    break;
		default:
			return false;
			break;
	}

	ac->Changed.connect (
	    plugin_connections, MISSING_INVALIDATOR, std::bind (plugin_mapping, _1, _2), this);
	ac->Changed (true, PBD::Controllable::GroupControlDisposition::UseGroup);
	return true;
}

bool
Console1::handle_plugin_parameter(const PluginParameterMapping& ppm, int32_t n_controls,
                                  const ParameterDescriptor& parameterDescriptor,
                                  const std::shared_ptr<AutomationControl>& ac)
{
    bool swtch = false;
    DEBUG_TRACE (DEBUG::Console1, string_compose ("\nName: %1 \n", parameterDescriptor.label));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Normal: %1 \n", parameterDescriptor.normal));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Lower: %1 \n", parameterDescriptor.lower));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Upper: %1 \n", parameterDescriptor.upper));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Toggled: %1 \n", parameterDescriptor.toggled));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Logarithmic: %1 \n", parameterDescriptor.logarithmic));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Rangesteps: %1 \n", parameterDescriptor.rangesteps));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Unit: %1 \n", parameterDescriptor.unit));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Step: %1 \n", parameterDescriptor.step));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Smallstep: %1 \n", parameterDescriptor.smallstep));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Largestep: %1 \n", parameterDescriptor.largestep));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Int-step: %1 \n", parameterDescriptor.integer_step));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Sr_dependent: %1 \n", parameterDescriptor.sr_dependent));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Enumeration: %1 \n", parameterDescriptor.enumeration));
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Inlinectrl: %1 \n", parameterDescriptor.inline_ctrl));

    if (parameterDescriptor.toggled)
	    swtch = true;
    else if (parameterDescriptor.integer_step && parameterDescriptor.upper == 1)
	    swtch = true;
    else if (ppm.is_switch)
	    swtch = true;

    return setup_plugin_controller(ppm, n_controls, parameterDescriptor, ac);
}

bool
Console1::remap_plugin_parameter (int plugin_index)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::remap_plugin_parameter index = %1 \n", plugin_index));
	//plugin_connections.drop_connections ();

	int32_t                    n_controls = -1;
	std::shared_ptr<Processor> proc       = find_plugin (plugin_index);
	set<Evoral::Parameter>     p    = proc->what_can_be_automated ();

    std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
	if (!plugin_insert)
		return false;

	std::shared_ptr<Plugin> plugin = plugin_insert->plugin ();
	if (!plugin)
		return false;

	setup_plugin_mute_button (plugin_insert);

	PluginMappingMap::iterator pmmit = plugin_mapping_map.find (plugin->unique_id ());
	if (pmmit == plugin_mapping_map.end ())
		return false;
	PluginMapping pluginMapping = pmmit->second;

	for (set<Evoral::Parameter>::iterator j = p.begin (); j != p.end (); ++j) {
		++n_controls;
		std::string n = proc->describe_parameter (*j);
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::remap_plugin_parameter: Plugin parameter %1: %2\n", n_controls, n));
		if (n == "hidden") {
			continue;
		}
		ParameterDescriptor parameterDescriptor;
		plugin->get_parameter_descriptor (n_controls, parameterDescriptor);
		PluginParameterMapping             ppm        = pluginMapping.parameters[n_controls];
		Controller             *controller = get_controller (ppm.controllerId);
		std::shared_ptr<AutomationControl> ac = plugin_insert->automation_control (Evoral::Parameter (PluginAutomation, 0, n_controls));
		if (controller && ac) {
			DEBUG_TRACE (DEBUG::Console1, string_compose ("CONTROLLER has plugin_action %1, has shitft_plugin_action %2\n", controller->get_plugin_action () ? "Yes" : "No", controller->get_plugin_shift_action () ? "Yes" : "No"));
			set_plugin_receive_connection (controller, ac, parameterDescriptor, ppm);
	    }
    }
	return true;
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
    DEBUG_TRACE (DEBUG::Console1, string_compose ("spill_plugins: Found plugin %1\n", proc->name ()));
    std::shared_ptr<PluginInsert> plugin_insert = std::dynamic_pointer_cast<PluginInsert> (proc);
    if (!plugin_insert)
        return false;

    std::shared_ptr<Plugin> plugin = plugin_insert->plugin ();
    if (!plugin)
        return false;

    DEBUG_TRACE (DEBUG::Console1, string_compose ("spill_plugins: Found plugin id %1\n", plugin->unique_id ()));

    // Setup mute button
    setup_plugin_mute_button(plugin_insert);

    PluginMappingMap::iterator pmmit = plugin_mapping_map.find (plugin->unique_id ());
    mapping_found = (pmmit != plugin_mapping_map.end ());

    if (!mapping_found) {
        if (create_mapping_stubs) {
            create_plugin_mapping_stubs (proc, plugin);
        }
        return true;
    }

    PluginMapping pluginMapping = pmmit->second;

    DEBUG_TRACE (DEBUG::Console1,
	         string_compose ("spill_plugins: Plugin mapping found for id %1, name %2\n", pluginMapping.id, pluginMapping.name));

    set<Evoral::Parameter> p = proc->what_can_be_automated ();

    for (set<Evoral::Parameter>::iterator j = p.begin (); j != p.end (); ++j) {
        ++n_controls;
        std::string n = proc->describe_parameter (*j);
	    DEBUG_TRACE (DEBUG::Console1, string_compose ("spill_plugins: Plugin parameter %1: %2\n", n_controls, n));
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
                handle_plugin_parameter(ppm, n_controls, parameterDescriptor, c);
            }
        }
    }
    return true;
}

Glib::RefPtr<Gtk::ListStore> Console1::getPluginControllerModel()
{
    plugin_controller_model = Gtk::ListStore::create (plugin_controller_columns);
	Gtk::TreeModel::Row plugin_controller_combo_row;
    for( const auto &controller : controllerNameIdMap )
			{
				plugin_controller_combo_row                                           = *(plugin_controller_model->append ());
				plugin_controller_combo_row[plugin_controller_columns.controllerId]   = controller.second;
				plugin_controller_combo_row[plugin_controller_columns.controllerName] = X_ (controller.first);
			}
	return plugin_controller_model;
}

} // namespace Console1
