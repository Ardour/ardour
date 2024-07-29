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

#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/cellrenderercombo.h>
#include <gtkmm/cellrenderertoggle.h>

#include <gtkmm/alignment.h>
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/strsplit.h"
#include "pbd/unwind.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/parameter_descriptor.h"
#include "console1.h"
#include "gtkmm2ext/action_model.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
Console1::get_gui () const
{
	if (!gui) {
		const_cast<Console1*> (this)->build_gui ();
	}
	static_cast<Gtk::Notebook*> (gui)->show_all ();
	return gui;
}

void
Console1::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget* w = static_cast<Gtk::Widget*> (gui)->get_parent ();
		if (w) {
			w->hide ();
			delete w;
		}
	}
	delete gui;
	gui = 0;
}

void
Console1::build_gui ()
{
	gui = new C1GUI (*this);
}

/*--------------------*/

C1GUI::C1GUI (Console1& p)
  : c1 (p)
  , table (6, 4)
  , swap_solo_mute_cb ()
  , create_plugin_stubs_btn ()
  , ignore_active_change (false)
{
	set_border_width (12);

	table.set_row_spacings (4);
	table.set_col_spacings (6);
	table.set_border_width (12);
	table.set_homogeneous (false);

	std::string data_file_path;
	string name = "console1.png";
	Searchpath spath (ARDOUR::ardour_data_search_path ());
	spath.add_subdirectory_to_paths ("icons");
	find_file (spath, name, data_file_path);
	if (!data_file_path.empty ()) {
		image.set (data_file_path);
		hpacker.pack_start (image, false, false);
	}

	Gtk::Label* l;
	int row = 0;

	input_combo.pack_start (midi_port_columns.short_name);
	output_combo.pack_start (midi_port_columns.short_name);

	input_combo.signal_changed ().connect (
	  sigc::bind (sigc::mem_fun (*this, &C1GUI::active_port_changed), &input_combo, true));
	output_combo.signal_changed ().connect (
	  sigc::bind (sigc::mem_fun (*this, &C1GUI::active_port_changed), &output_combo, false));

	// swap_solo_mute (_ ("Swap Solo and Mute"));
	swap_solo_mute_cb.set_tooltip_text (
	  _ ("If checked Ardour the mute and solo buttons are swept so they have the same order as in the GUI."));
	swap_solo_mute_cb.set_active (p.swap_solo_mute);
	swap_solo_mute_cb.signal_toggled ().connect (sigc::mem_fun (*this, &C1GUI::set_swap_solo_mute));

	// create_plugin_stubs (_ ("Create Plugin Mapping Stubs"));
	create_plugin_stubs_btn.set_tooltip_text (_ ("If checked a mapping stub is created for every unknown plugin."));
	create_plugin_stubs_btn.set_active (p.create_mapping_stubs);
	create_plugin_stubs_btn.signal_toggled ().connect (sigc::mem_fun (*this, &C1GUI::set_create_mapping_stubs));

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _ ("Incoming MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0));
	table.attach (input_combo, 1, 2, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _ ("Outgoing MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0));
	table.attach (output_combo, 1, 2, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _ ("Swap Solo and Mute:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0));
	table.attach (swap_solo_mute_cb, 1, 2, row, row + 1);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _ ("Create Plugin Mapping Stubs:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row + 1, AttachOptions (FILL | EXPAND), AttachOptions (0));
	table.attach (create_plugin_stubs_btn, 1, 2, row, row + 1);
	row++;

	hpacker.pack_start (table, true, true);
  	append_page (hpacker, _("Device Setup"));
	hpacker.show_all();

    // Create the page for plugin mappings
    p.load_mappings ();

  	VBox* plugconfig_packer = manage (new VBox);
	HBox* plugselect_packer = manage (new HBox);

	l = manage (new Gtk::Label (_("Select Plugin")));
  	plugselect_packer->pack_start (*l, false, false);

	plugconfig_packer->pack_start (*plugselect_packer, false, false);
    
    Glib::RefPtr<Gtk::ListStore> plugin_store_model = ListStore::create (plugin_columns);
	TreeModel::Row plugin_combo_row;
    for( const auto &pm : c1.getPluginMappingMap() ){
        plugin_combo_row = *(plugin_store_model->append ());
		plugin_combo_row[plugin_columns.plugin_name] = pm.second.name;
		plugin_combo_row[plugin_columns.plugin_id] = pm.first;
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Add Plugin: name %1 / %2\n", pm.second.name, pm.first));
	}
	plugins_combo.pack_start (plugin_columns.plugin_name);
	plugins_combo.signal_changed ().connect (
	  sigc::bind (sigc::mem_fun (*this, &C1GUI::active_plugin_changed), &plugins_combo));
	plugins_combo.set_model (plugin_store_model);

	plugselect_packer->pack_start (plugins_combo, true, true);
	plugin_mapping_scroller.property_shadow_type() = Gtk::SHADOW_NONE;
    plugin_mapping_scroller.set_policy(Gtk::PolicyType::POLICY_AUTOMATIC, Gtk::PolicyType::POLICY_AUTOMATIC);

	plugin_mapping_scroller.add (plugin_assignment_editor);
	plugconfig_packer->pack_start (plugin_mapping_scroller, true, true, 20);

	build_plugin_assignment_editor ();

	append_page (*plugconfig_packer, _ ("Plugin Mappings"));
	plugconfig_packer->show_all ();

	/* update the port connection combos */

	update_port_combos ();

	/* catch future changes to connection state */

	ARDOUR::AudioEngine::instance ()->PortRegisteredOrUnregistered.connect (
	  _port_connections, invalidator (*this), boost::bind (&C1GUI::connection_handler, this), gui_context ());
	ARDOUR::AudioEngine::instance ()->PortPrettyNameChanged.connect (
	  _port_connections, invalidator (*this), boost::bind (&C1GUI::connection_handler, this), gui_context ());
	c1.ConnectionChange.connect (
	  _port_connections, invalidator (*this), boost::bind (&C1GUI::connection_handler, this), gui_context ());
}

C1GUI::~C1GUI () {
    write_plugin_assignment();
}

void
C1GUI::set_swap_solo_mute ()
{
	c1.swap_solo_mute = !c1.swap_solo_mute;
}

void
C1GUI::set_create_mapping_stubs ()
{
	c1.create_mapping_stubs = !c1.create_mapping_stubs;
}

void
C1GUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (ignore_active_change, true);

	update_port_combos ();
}

void
C1GUI::update_port_combos ()
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance ()->get_ports (
	  "", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput | ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance ()->get_ports (
	  "", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput | ARDOUR::IsTerminal), midi_outputs);

	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	input_combo.set_model (input);
	output_combo.set_model (output);

	Gtk::TreeModel::Children children = input->children ();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin ();
	++i; /* skip "Disconnected" */

	for (n = 1; i != children.end (); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (c1.input_port ()->connected_to (port_name)) {
			input_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		input_combo.set_active (0); /* disconnected */
	}

	children = output->children ();
	i = children.begin ();
	++i; /* skip "Disconnected" */

	for (n = 1; i != children.end (); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (c1.output_port ()->connected_to (port_name)) {
			output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo.set_active (0); /* disconnected */
	}
}

Glib::RefPtr<Gtk::ListStore>
C1GUI::build_midi_port_list (vector<string> const& ports, bool for_input)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (midi_port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[midi_port_columns.full_name] = string ();
	row[midi_port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin (); p != ports.end (); ++p) {
		row = *store->append ();
		row[midi_port_columns.full_name] = *p;
		std::string pn = ARDOUR::AudioEngine::instance ()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[midi_port_columns.short_name] = pn;
	}

	return store;
}

void
C1GUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty ()) {
		if (for_input) {
			c1.input_port ()->disconnect_all ();
		} else {
			c1.output_port ()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!c1.input_port ()->connected_to (new_port)) {
			c1.input_port ()->disconnect_all ();
			c1.input_port ()->connect (new_port);
		}
	} else {
		if (!c1.output_port ()->connected_to (new_port)) {
			c1.output_port ()->disconnect_all ();
			c1.output_port ()->connect (new_port);
		}
	}
}

void
C1GUI::change_controller (const Glib::ustring &sPath, const TreeModel::iterator &iter)
{
	Gtk::TreePath path(sPath);
	Gtk::TreeModel::iterator row = plugin_assignment_store->get_iter(path);
	int index = *path.begin ();
	if (row) {

		string controllerName = (*iter)[c1.plugin_controller_columns.controllerName];
	    int controllerId = (*iter)[c1.plugin_controller_columns.controllerId];
    	pc.parameters[index].controllerId = ArdourSurface::Console1::ControllerID (controllerId);
		(*row).set_value (plugin_assignment_editor_columns.controllerName, controllerName);
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("Column Name: Controller, index %1, name %2 \n", index, controllerName));
		assignement_changed = true;
	}
}

void C1GUI::toggle_shift( const Glib::ustring& s){
	int index = atoi (s.c_str());
	Gtk::TreeModel::iterator row = plugin_assignment_store->get_iter (s);
    if( row )
    {
		bool value = !pc.parameters[index].shift;
		pc.parameters[index].shift = value;
		(*row).set_value (plugin_assignment_editor_columns.shift, value);
	    DEBUG_TRACE (DEBUG::Console1, string_compose ("Column Name: Shift, value %1\n", value));
		assignement_changed = true;
	}
}

CellRendererCombo*
C1GUI::make_action_renderer (Glib::RefPtr<ListStore> model, Gtk::TreeModelColumnBase column)
{
	CellRendererCombo* renderer = manage (new CellRendererCombo);
	renderer->property_model() = model;
	renderer->property_editable() = true;
	renderer->property_text_column () = 0;
	renderer->property_has_entry () = false;
	renderer->signal_changed().connect (sigc::mem_fun(*this, &C1GUI::change_controller));

	return renderer;
}

void
C1GUI::build_plugin_assignment_editor ()
{
	plugin_assignment_editor.append_column (_("Key"), plugin_assignment_editor_columns.index);
	plugin_assignment_editor.append_column (_("Name"), plugin_assignment_editor_columns.name);
	plugin_assignment_editor.append_column (_("Switch"), plugin_assignment_editor_columns.is_switch);

	TreeViewColumn* col;
	CellRendererCombo* renderer;

	CellRendererToggle* boolRenderer = manage (new CellRendererToggle);
	boolRenderer->set_active ();
	boolRenderer->property_activatable() = true;
	col = manage (new TreeViewColumn (_ ("Shift"), *boolRenderer));
	col->add_attribute (boolRenderer->property_active (), plugin_assignment_editor_columns.shift);
	boolRenderer->signal_toggled().connect (sigc::mem_fun(*this, &C1GUI::toggle_shift));
	plugin_assignment_editor.append_column (*col);


	renderer = make_action_renderer (c1.getPluginControllerModel(), plugin_assignment_editor_columns.controllerName);
	col = manage (new TreeViewColumn (_("Control"), *renderer));
	col->add_attribute (renderer->property_text(), plugin_assignment_editor_columns.controllerName);
	plugin_assignment_editor.append_column (*col);

	plugin_assignment_store = ListStore::create (plugin_assignment_editor_columns);
	plugin_assignment_editor.set_model (plugin_assignment_store);
}


void
C1GUI::active_plugin_changed(Gtk::ComboBox* combo ){
    DEBUG_TRACE (DEBUG::Console1, "C1GUI active_plugin_changed\n");

	write_plugin_assignment ();

	plugin_assignment_editor.set_model (Glib::RefPtr<TreeModel>());
	plugin_assignment_store->clear ();

	TreeModel::iterator active = combo->get_active ();
	TreeModel::Row plugin_assignment_row;

	string new_plugin_name = (*active)[plugin_columns.plugin_name];
	string new_plugin_id = (*active)[plugin_columns.plugin_id];
    DEBUG_TRACE (DEBUG::Console1, string_compose ("Plugin: selected %1 / %2\n", new_plugin_name, new_plugin_id));
	pc = c1.getPluginMappingMap ()[new_plugin_id];

    for( auto &parm : pc.parameters ){
		plugin_assignment_row = *(plugin_assignment_store->append ());
		plugin_assignment_row[plugin_assignment_editor_columns.index] = parm.first;
		plugin_assignment_row[plugin_assignment_editor_columns.name] = parm.second.name;
		plugin_assignment_row[plugin_assignment_editor_columns.controllerName] = c1.findControllerNameById(parm.second.controllerId);
		plugin_assignment_row[plugin_assignment_editor_columns.is_switch] = parm.second.is_switch;
		plugin_assignment_row[plugin_assignment_editor_columns.shift] = parm.second.shift;

		DEBUG_TRACE (DEBUG::Console1, string_compose ("Parameter Name %1 \n", parm.second.name));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Parameter Index: %1 - index %2 \n", parm.first, parm.second.paramIndex));
        DEBUG_TRACE (DEBUG::Console1, string_compose ("ControllerId: %1 \n", parm.second.controllerId));
        DEBUG_TRACE (DEBUG::Console1, string_compose ("is switch? %1 \n", parm.second.is_switch));
        DEBUG_TRACE (DEBUG::Console1, string_compose ("is shift? %1 \n", parm.second.shift));
    }
	plugin_assignment_editor.set_model (plugin_assignment_store);

}

void C1GUI::write_plugin_assignment(){
    DEBUG_TRACE (DEBUG::Console1, "write_plugin_assignment\n");
    if( !assignement_changed )
		return;
	c1.write_plugin_mapping (pc);
	assignement_changed = false;
}
