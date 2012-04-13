/*
	Copyright (C) 2010 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treestore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/cellrenderercombo.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "mackie_control_protocol.h"
#include "device_info.h"
#include "gui.h"

#include "i18n.h"

using namespace std;
using namespace Mackie;
using namespace Gtk;

void*
MackieControlProtocol::get_gui () const
{
	if (!_gui) {
		const_cast<MackieControlProtocol*>(this)->build_gui ();
	}

	return _gui;
}

void
MackieControlProtocol::tear_down_gui ()
{
	delete (MackieControlProtocolGUI*) _gui;
}

void
MackieControlProtocol::build_gui ()
{
	_gui = (void *) new MackieControlProtocolGUI (*this);
}

MackieControlProtocolGUI::MackieControlProtocolGUI (MackieControlProtocol& p)
	: _cp (p)
{
	set_border_width (12);

	Gtk::Table* table = Gtk::manage (new Gtk::Table (2, 2));
	table->set_spacings (4);
	
	table->attach (*manage (new Gtk::Label (_("Surface type:"))), 0, 1, 0, 1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (_surface_combo, 1, 2, 0, 1, AttachOptions(FILL|EXPAND), AttachOptions(0));

	vector<string> surfaces;
	
	for (std::map<std::string,DeviceInfo>::iterator i = DeviceInfo::device_info.begin(); i != DeviceInfo::device_info.end(); ++i) {
		std::cerr << "Dveice known: " << i->first << endl;
		surfaces.push_back (i->first);
	}
	Gtkmm2ext::set_popdown_strings (_surface_combo, surfaces);
	_surface_combo.set_active_text (p.device_info().name());
	_surface_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::surface_combo_changed));

	append_page (*table, _("Device Selection"));
	table->show_all();

	/* function key editor */

	append_page (function_key_scroller, _("Function Keys"));
	function_key_scroller.add (function_key_editor);
	
	rebuild_function_key_editor ();
	function_key_scroller.show_all();
}

void
MackieControlProtocolGUI::rebuild_function_key_editor ()
{
	/* build a model of all available actions (needs to be tree structured
	 * more) 
	 */

	available_action_model = TreeStore::create (available_action_columns);

	vector<string> a_names;
	vector<string> a_paths;
	vector<string> a_tooltips;
	vector<string> a_keys;
	vector<Gtk::AccelKey> a_bindings;

	ActionManager::get_all_actions (a_names, a_paths, a_tooltips, a_keys, a_bindings);

	vector<string>::iterator n = a_names.begin();
	vector<string>::iterator p = a_paths.begin();
	TreeModel::Row r;

	for (; n != a_names.end(); ++n, ++p) {
		r = *(available_action_model->append());
		r[available_action_columns.name] = (*n);
		r[available_action_columns.path] = (*p);
	}

	function_key_editor.append_column (_("Key"), function_key_columns.name);

	CellRendererCombo* action_renderer = manage (new CellRendererCombo);
	action_renderer->property_model() = available_action_model;
	action_renderer->property_editable() = true;
	action_renderer->property_text_column() = 1;
	action_renderer->property_has_entry() = false;

	TreeViewColumn* col;

	col = manage (new TreeViewColumn (_("Plain"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.plain);
	function_key_editor.append_column (*col);
	
	col = manage (new TreeViewColumn (_("Shift"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.shift);
	function_key_editor.append_column (*col);

	col = manage (new TreeViewColumn (_("Control"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.control);
	function_key_editor.append_column (*col);

	col = manage (new TreeViewColumn (_("Option"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.option);
	function_key_editor.append_column (*col);

	col = manage (new TreeViewColumn (_("Cmd/Alt"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.cmdalt);
	function_key_editor.append_column (*col);

	col = manage (new TreeViewColumn (_("Shift+Control"), *action_renderer));
	col->add_attribute (action_renderer->property_text(), function_key_columns.shiftcontrol);
	function_key_editor.append_column (*col);

	/* now fill with data */

	function_key_model = ListStore::create (function_key_columns);

	r = *(function_key_model->append());
	r[function_key_columns.name] = "F1";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F2";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F3";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F4";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F5";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F6";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F7";
	r = *(function_key_model->append());
	r[function_key_columns.name] = "F8";

	function_key_editor.set_model (function_key_model);
}

void
MackieControlProtocolGUI::surface_combo_changed ()
{
	_cp.set_device (_surface_combo.get_active_text());
}


