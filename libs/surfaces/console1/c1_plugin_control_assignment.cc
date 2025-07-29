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

#include "c1_gui.h"

#include "ardour/debug.h"
#include "console1.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace std;


namespace Console1
{

VBox*
C1GUI::build_plugin_assignment_page ()
{
	VBox* plugconfig_packer = manage (new VBox);
	HBox* plugselect_packer = manage (new HBox);

	Gtk::Label* l;
	l = manage (new Gtk::Label (_ ("Select Plugin")));
	plugselect_packer->pack_start (*l, false, false);

	plugconfig_packer->pack_start (*plugselect_packer, false, false);

	Glib::RefPtr<Gtk::ListStore> plugin_store_model = ListStore::create (plugin_columns);
	TreeModel::Row               plugin_combo_row;
	for (const auto& pm : c1.getPluginMappingMap ()) {
		plugin_combo_row                             = *(plugin_store_model->append ());
		plugin_combo_row[plugin_columns.plugin_name] = pm.second.name;
		plugin_combo_row[plugin_columns.plugin_id]   = pm.first;
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Add Plugin: name %1 / %2\n", pm.second.name, pm.first));
	}
	plugins_combo.pack_start (plugin_columns.plugin_name);
	plugins_combo.signal_changed ().connect (
	    sigc::bind (sigc::mem_fun (*this, &C1GUI::active_plugin_changed), &plugins_combo));
	plugins_combo.set_model (plugin_store_model);

	plugselect_packer->pack_start (plugins_combo, true, true);
	plugin_mapping_scroller.property_shadow_type () = Gtk::SHADOW_NONE;
	plugin_mapping_scroller.set_policy (Gtk::PolicyType::POLICY_AUTOMATIC, Gtk::PolicyType::POLICY_AUTOMATIC);

	plugin_mapping_scroller.add (plugin_assignment_editor);
	plugconfig_packer->pack_start (plugin_mapping_scroller, true, true, 20);

	build_plugin_assignment_editor ();

	midi_assign_button = manage (new ToggleButton (_ ("assign Control per MIDI")));
	midi_assign_button->set_sensitive (false);
	midi_assign_button->set_active (false);
	midi_assign_button->signal_toggled ().connect (sigc::bind (sigc::mem_fun (*this, &C1GUI::midi_assign_button_toggled), midi_assign_button));
	plugconfig_packer->pack_start (*midi_assign_button, false, false);
    plugin_assignment_changed.connect (sigc::mem_fun (*this, &C1GUI::write_plugin_assignment));
	return plugconfig_packer;
}

void
C1GUI::build_plugin_assignment_editor ()
{
	plugin_assignment_editor.append_column (_ ("Key"), plugin_assignment_editor_columns.index);
	plugin_assignment_editor.append_column (_ ("Name"), plugin_assignment_editor_columns.name);
	plugin_assignment_editor.append_column (_ ("Switch"), plugin_assignment_editor_columns.is_switch);

	TreeViewColumn*    col;
	CellRendererCombo* controlRenderer;

	CellRendererToggle* boolRenderer = manage (new CellRendererToggle);
	boolRenderer->set_active ();
	boolRenderer->property_activatable () = true;
	col                                   = manage (new TreeViewColumn (_ ("Shift"), *boolRenderer));
	col->add_attribute (boolRenderer->property_active (), plugin_assignment_editor_columns.shift);
	boolRenderer->signal_toggled ().connect (sigc::mem_fun (*this, &C1GUI::toggle_shift));
	plugin_assignment_editor.append_column (*col);

	controlRenderer = make_action_renderer (c1.getPluginControllerModel (), plugin_assignment_editor_columns.controllerName);
	col             = manage (new TreeViewColumn (_ ("Control"), *controlRenderer));
	col->add_attribute (controlRenderer->property_text (), plugin_assignment_editor_columns.controllerName);
	plugin_assignment_editor.append_column (*col);

	plugin_assignment_store = ListStore::create (plugin_assignment_editor_columns);
	plugin_assignment_editor.set_model (plugin_assignment_store);
}

void
C1GUI::active_plugin_changed (Gtk::ComboBox* combo)
{
	DEBUG_TRACE (DEBUG::Console1, "C1GUI active_plugin_changed\n");

	write_plugin_assignment ();

	plugin_assignment_editor.set_model (Glib::RefPtr<TreeModel> ());
	plugin_assignment_store->clear ();

	TreeModel::iterator active = combo->get_active ();
	TreeModel::Row      plugin_assignment_row;

	string new_plugin_name = (*active)[plugin_columns.plugin_name];
	string new_plugin_id   = (*active)[plugin_columns.plugin_id];
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Plugin: selected %1 / %2\n", new_plugin_name, new_plugin_id));
	pc = c1.getPluginMappingMap ()[new_plugin_id];

	for (auto& parm : pc.parameters) {
		plugin_assignment_row                                                  = *(plugin_assignment_store->append ());
		plugin_assignment_row[plugin_assignment_editor_columns.index]          = parm.first;
		plugin_assignment_row[plugin_assignment_editor_columns.name]           = parm.second.name;
		plugin_assignment_row[plugin_assignment_editor_columns.controllerName] = c1.findControllerNameById (parm.second.controllerId);
		plugin_assignment_row[plugin_assignment_editor_columns.is_switch]      = parm.second.is_switch;
		plugin_assignment_row[plugin_assignment_editor_columns.shift]          = parm.second.shift;

		DEBUG_TRACE (DEBUG::Console1, string_compose ("Parameter Name %1 \n", parm.second.name));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Parameter Index: %1 - index %2 \n", parm.first, parm.second.paramIndex));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("ControllerId: %1 \n", parm.second.controllerId));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("is switch? %1 \n", parm.second.is_switch));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("is shift? %1 \n", parm.second.shift));
	}
	plugin_assignment_editor.set_model (plugin_assignment_store);
	plugin_assignment_editor.get_selection ()->set_mode (SELECTION_SINGLE);
	plugin_assignment_editor.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &C1GUI::plugin_assignment_editor_selection_changed));
	midi_assign_button->set_sensitive (false);
	midi_assign_button->set_active (false);
}

CellRendererCombo*
C1GUI::make_action_renderer (Glib::RefPtr<ListStore> model, Gtk::TreeModelColumnBase column)
{
	CellRendererCombo* renderer       = manage (new CellRendererCombo);
	renderer->property_model ()       = model;
	renderer->property_editable ()    = true;
	renderer->property_text_column () = 0;
	renderer->property_has_entry ()   = false;
	renderer->signal_changed ().connect (sigc::mem_fun (*this, &C1GUI::change_controller));

	return renderer;
}

void
C1GUI::change_controller (const Glib::ustring& sPath, const TreeModel::iterator& iter)
{
	Gtk::TreePath            path (sPath);
	Gtk::TreeModel::iterator row   = plugin_assignment_store->get_iter (path);
	int                      index = *path.begin ();
	if (row) {
		string controllerName             = (*iter)[c1.plugin_controller_columns.controllerName];
		int    controllerId               = (*iter)[c1.plugin_controller_columns.controllerId];
		pc.parameters[index].controllerId = Console1::ControllerID (controllerId);
		(*row).set_value (plugin_assignment_editor_columns.controllerName, controllerName);
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("Column Name: Controller, index %1, name %2 \n", index, controllerName));
		plugin_assignment_changed ();
	}
}

void
C1GUI::plugin_assignment_editor_selection_changed ()
{
	if (plugin_assignment_editor.get_selection ()->count_selected_rows () != 1) {
		midi_assign_button->set_sensitive (false);
	}
	midi_assign_button->set_sensitive (true);
}

void
C1GUI::write_plugin_assignment ()
{
	DEBUG_TRACE (DEBUG::Console1, "write_plugin_assignment\n");
	c1.write_plugin_mapping (pc);
}

void 
C1GUI::change_controller_number( int controllerNumber, bool shiftState ){
	DEBUG_TRACE (DEBUG::Console1, string_compose ("C1GUI::change_controller_number: received %1\n", controllerNumber));
	Gtk::TreeModel::iterator row = plugin_assignment_editor.get_selection ()->get_selected ();

	if (row) {
		string name = c1.findControllerNameById (Console1::ControllerID(controllerNumber));
		(*row).set_value (plugin_assignment_editor_columns.controllerName, name);
		(*row).set_value (plugin_assignment_editor_columns.shift, shiftState);
		int index                         = (*row).get_value (plugin_assignment_editor_columns.index);
		pc.parameters[index].controllerId = Console1::ControllerID (controllerNumber);
		pc.parameters[index].shift        = shiftState ? 1 : 0;
		plugin_assignment_changed ();
	}
	midi_assign_button->set_active (false);
	midi_assign_button->set_sensitive (false);
}

void
C1GUI::midi_assign_button_toggled (Gtk::ToggleButton* b)
{
	DEBUG_TRACE (DEBUG::Console1, "C1GUI::midi_assign_button_changed() \n");
	bool en = b->get_active ();
	c1.midi_assign_mode = en;
    if( en )
        c1.SendControllerNumber.connect (std::bind ( &C1GUI::change_controller_number, this, _1, _2));
}

void
C1GUI::toggle_shift (const Glib::ustring& s)
{
	int                      index = atoi (s.c_str ());
	Gtk::TreeModel::iterator row   = plugin_assignment_store->get_iter (s);
	if (row) {
		bool value                 = !pc.parameters[index].shift;
		pc.parameters[index].shift = value;
		(*row).set_value (plugin_assignment_editor_columns.shift, value);
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Column Name: Shift, value %1\n", value));
		plugin_assignment_changed ();
	}
}

}