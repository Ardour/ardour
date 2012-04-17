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

#include "pbd/strsplit.h"

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
	
	table->attach (*manage (new Gtk::Label (_("Device Type:"))), 0, 1, 0, 1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (_surface_combo, 1, 2, 0, 1, AttachOptions(FILL|EXPAND), AttachOptions(0));

	table->attach (*manage (new Gtk::Label (_("Profile/Settings:"))), 0, 1, 1, 2, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (_profile_combo, 1, 2, 1, 2, AttachOptions(FILL|EXPAND), AttachOptions(0));

	vector<string> surfaces;
	
	for (std::map<std::string,DeviceInfo>::iterator i = DeviceInfo::device_info.begin(); i != DeviceInfo::device_info.end(); ++i) {
		surfaces.push_back (i->first);
	}
	Gtkmm2ext::set_popdown_strings (_surface_combo, surfaces);
	_surface_combo.set_active_text (p.device_info().name());
	_surface_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::surface_combo_changed));

	vector<string> profiles;
	
	profiles.push_back ("default");

	for (std::map<std::string,DeviceProfile>::iterator i = DeviceProfile::device_profiles.begin(); i != DeviceProfile::device_profiles.end(); ++i) {
		profiles.push_back (i->first);
	}
	Gtkmm2ext::set_popdown_strings (_profile_combo, profiles);
	_profile_combo.set_active_text (p.device_profile().name());
	_profile_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::profile_combo_changed));

	append_page (*table, _("Device Selection"));
	table->show_all();

	/* function key editor */

	append_page (function_key_scroller, _("Function Keys"));
	function_key_scroller.add (function_key_editor);
	
	build_available_action_menu ();
	build_function_key_editor ();
	refresh_function_key_editor ();
	function_key_scroller.show_all();
}

CellRendererCombo*
MackieControlProtocolGUI::make_action_renderer (Glib::RefPtr<TreeStore> model, Gtk::TreeModelColumnBase column)
{
	CellRendererCombo* renderer = manage (new CellRendererCombo);
	renderer->property_model() = model;
	renderer->property_editable() = true;
	renderer->property_text_column() = 0;
	renderer->property_has_entry() = false;
	renderer->signal_edited().connect (sigc::bind (sigc::mem_fun(*this, &MackieControlProtocolGUI::action_changed), column));

	return renderer;
}

void
MackieControlProtocolGUI::build_available_action_menu ()
{
	/* build a model of all available actions (needs to be tree structured
	 * more) 
	 */

	available_action_model = TreeStore::create (available_action_columns);

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<AccelKey> bindings;
	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;

	ActionManager::get_all_actions (labels, paths, tooltips, keys, bindings);

	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;

	available_action_model->clear ();

	for (l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l) {

		TreeModel::Row row;
		vector<string> parts;

		parts.clear ();

		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ( parts[1] == _("Main_menu") )
			continue;
		if ( parts[1] == _("JACK") )
			continue;
		if ( parts[1] == _("redirectmenu") )
			continue;
		if ( parts[1] == _("Editor_menus") )
			continue;
		if ( parts[1] == _("RegionList") )
			continue;
		if ( parts[1] == _("ProcessorMenu") )
			continue;

		if ((r = nodes.find (parts[1])) == nodes.end()) {

			/* top level is missing */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = available_action_model->append();
			nodes[parts[1]] = rowp;
			parent = *(rowp);
			parent[available_action_columns.name] = parts[1];

			row = *(available_action_model->append (parent.children()));

		} else {

			row = *(available_action_model->append ((*r->second)->children()));

		}

		/* add this action */

		if (l->empty ()) {
			row[available_action_columns.name] = *t;
			action_map[*t] = *p;
		} else {
			row[available_action_columns.name] = *l;
			action_map[*l] = *p;
		}

		row[available_action_columns.path] = (*p);
	}
}

void
MackieControlProtocolGUI::build_function_key_editor ()
{
	function_key_editor.append_column (_("Key"), function_key_columns.name);

	TreeViewColumn* col;
	CellRendererCombo* renderer;

	renderer = make_action_renderer (available_action_model, function_key_columns.plain);
	col = manage (new TreeViewColumn (_("Plain"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.plain);
	function_key_editor.append_column (*col);
	
	renderer = make_action_renderer (available_action_model, function_key_columns.shift);
	col = manage (new TreeViewColumn (_("Shift"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.shift);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (available_action_model, function_key_columns.control);
	col = manage (new TreeViewColumn (_("Control"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.control);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (available_action_model, function_key_columns.option);
	col = manage (new TreeViewColumn (_("Option"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.option);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (available_action_model, function_key_columns.cmdalt);
	col = manage (new TreeViewColumn (_("Cmd/Alt"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.cmdalt);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (available_action_model, function_key_columns.shiftcontrol);
	col = manage (new TreeViewColumn (_("Shift+Control"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.shiftcontrol);
	function_key_editor.append_column (*col);

	function_key_model = ListStore::create (function_key_columns);
	function_key_editor.set_model (function_key_model);
}

void
MackieControlProtocolGUI::refresh_function_key_editor ()
{
	function_key_editor.set_model (Glib::RefPtr<TreeModel>());
	function_key_model->clear ();

	/* now fill with data */

	TreeModel::Row row;
	DeviceProfile dp (_cp.device_profile());

	for (int n = 0; n < Mackie::Button::FinalGlobalButton; ++n) {

		Mackie::Button::ID bid = (Mackie::Button::ID) n;

		row = *(function_key_model->append());
		row[function_key_columns.name] = Mackie::Button::id_to_name (bid);
		row[function_key_columns.id] = bid;

		Glib::RefPtr<Gtk::Action> act;
		string action;
		const string defstring = "def";

		action = dp.get_button_action (bid, 0);
		if (action.empty()) {
			row[function_key_columns.plain] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.plain] = act->get_label();
			} else {
				row[function_key_columns.plain] = defstring;
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_CONTROL);
		if (action.empty()) {
			row[function_key_columns.control] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.control] = act->get_label();
			} else {
				row[function_key_columns.control] = defstring;
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_SHIFT);
		if (action.empty()) {
			row[function_key_columns.shift] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.shift] = act->get_label();
			} else {
				row[function_key_columns.shift] = defstring;
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_OPTION);
		if (action.empty()) {
			row[function_key_columns.option] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.option] = act->get_label();
			} else {
				row[function_key_columns.option] = defstring;
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_CMDALT);
		if (action.empty()) {
			row[function_key_columns.cmdalt] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.cmdalt] = act->get_label();
			} else {
				row[function_key_columns.cmdalt] = defstring;
			}
		}

		action = dp.get_button_action (bid, (MackieControlProtocol::MODIFIER_SHIFT|MackieControlProtocol::MODIFIER_CONTROL));
		if (action.empty()) {
			row[function_key_columns.shiftcontrol] = defstring;
		} else {
			act = ActionManager::get_action (action.c_str());
			if (act) {
				row[function_key_columns.shiftcontrol] = act->get_label();
			} else {
				row[function_key_columns.shiftcontrol] = defstring;
			}
		}
	}

	function_key_editor.set_model (function_key_model);
}

void 
MackieControlProtocolGUI::action_changed (const Glib::ustring &sPath, const Glib::ustring &text, TreeModelColumnBase col)
{
	Gtk::TreePath path(sPath);
	Gtk::TreeModel::iterator row = function_key_model->get_iter(path);

	if (row) {

		std::map<std::string,std::string>::iterator i = action_map.find (text);

		if (i == action_map.end()) {
			return;
		}

		Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (i->second.c_str());

		if (act) {
			(*row).set_value (col.index(), text);
		}
			
	}
}

void
MackieControlProtocolGUI::surface_combo_changed ()
{
	_cp.set_device (_surface_combo.get_active_text());
}

void
MackieControlProtocolGUI::profile_combo_changed ()
{
	_cp.set_profile (_profile_combo.get_active_text());
	refresh_function_key_editor ();
}


