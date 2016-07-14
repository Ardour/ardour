/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "gtkmm2ext/utils.h"

#include "lua_script_manager.h"
#include "script_selector.h"
#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

LuaScriptManager::LuaScriptManager ()
	: ArdourWindow (_("Script Manager"))
	, _a_set_button (_("Add/Set"))
	, _a_del_button (_("Remove"))
	, _a_edit_button (_("Edit"))
	, _a_call_button (_("Call"))
	, _c_add_button (_("New Hook"))
	, _c_del_button (_("Remove"))
{
	/* action script page */
	_a_store = ListStore::create (_a_model);
	_a_view.set_model (_a_store);
	_a_view.append_column (_("Action"), _a_model.action);
	_a_view.append_column (_("Name"), _a_model.name);
	_a_view.get_column(0)->set_resizable (true);
	_a_view.get_column(0)->set_expand (true);
	_a_view.get_column(1)->set_resizable (true);
	_a_view.get_column(1)->set_expand (true);

	Gtk::HBox* edit_box = manage (new Gtk::HBox);
	edit_box->set_spacing(3);

	edit_box->pack_start (_a_set_button, true, true);
	edit_box->pack_start (_a_del_button, true, true);
	edit_box->pack_start (_a_edit_button, true, true);
	edit_box->pack_start (_a_call_button, true, true);

	_a_set_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::set_action_btn_clicked));
	_a_del_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::del_action_btn_clicked));
	_a_edit_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::edit_action_btn_clicked));
	_a_call_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::call_action_btn_clicked));
	_a_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &LuaScriptManager::action_selection_changed));

	LuaInstance::instance()->ActionChanged.connect (sigc::mem_fun (*this, &LuaScriptManager::set_action_script_name));
	LuaInstance::instance()->SlotChanged.connect (sigc::mem_fun (*this, &LuaScriptManager::set_callback_script_name));

	Gtk::VBox *vbox = manage (new VBox());
	vbox->pack_start (_a_view, false, false);
	vbox->pack_end (*edit_box, false, false);
	vbox->show_all ();

	pages.pages ().push_back (Notebook_Helpers::TabElem (*vbox, "Action Scripts"));

	/* action hooks page */

	_c_store = ListStore::create (_c_model);
	_c_view.set_model (_c_store);
	_c_view.append_column (_("Name"), _c_model.name);
	_c_view.append_column (_("Signal(s)"), _c_model.signals);
	_c_view.get_column(0)->set_resizable (true);
	_c_view.get_column(0)->set_expand (true);
	_c_view.get_column(1)->set_resizable (true);
	_c_view.get_column(1)->set_expand (true);
	Gtk::CellRendererText* r = dynamic_cast<Gtk::CellRendererText*>(_c_view.get_column_cell_renderer (1));
	r->property_ellipsize () = Pango::ELLIPSIZE_MIDDLE;

	edit_box = manage (new Gtk::HBox);
	edit_box->set_spacing(3);
	edit_box->pack_start (_c_add_button, true, true);
	edit_box->pack_start (_c_del_button, true, true);

	_c_add_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::add_callback_btn_clicked));
	_c_del_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::del_callback_btn_clicked));
	_c_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &LuaScriptManager::callback_selection_changed));

	vbox = manage (new VBox());
	vbox->pack_start (_c_view, false, false);
	vbox->pack_end (*edit_box, false, false);
	vbox->show_all ();

	pages.pages ().push_back (Notebook_Helpers::TabElem (*vbox, "Action Hooks"));


	add (pages);
	pages.show();

	setup_actions ();
	setup_callbacks ();

	action_selection_changed ();
	callback_selection_changed ();
}

void
LuaScriptManager::session_going_away ()
{
	ArdourWindow::session_going_away ();
	hide_all();
}

void
LuaScriptManager::setup_actions ()
{
	LuaInstance *li = LuaInstance::instance();
	for (int i = 0; i < 9; ++i) {
		std::string name;
		TreeModel::Row r = *_a_store->append ();
		r[_a_model.id] = i;
		r[_a_model.action] = string_compose (_("Action %1"), i + 1);
		if (li->lua_action_name (i, name)) {
			r[_a_model.name] = name;
			r[_a_model.enabled] = true;
		} else {
			r[_a_model.name] = _("Unset");
			r[_a_model.enabled] = false;
		}
	}
}

void
LuaScriptManager::action_selection_changed ()
{
	TreeModel::Row row = *(_a_view.get_selection()->get_selected());
	if (row) {
		_a_set_button.set_sensitive (true);
	}
	else {
		_a_set_button.set_sensitive (false);
	}

	if (row && row[_a_model.enabled]) {
		_a_del_button.set_sensitive (true);
		_a_edit_button.set_sensitive (false); // TODO
		_a_call_button.set_sensitive (true);
	} else {
		_a_del_button.set_sensitive (false);
		_a_edit_button.set_sensitive (false);
		_a_call_button.set_sensitive (false);
	}
}

void
LuaScriptManager::set_action_btn_clicked ()
{
	TreeModel::Row row = *(_a_view.get_selection()->get_selected());
	assert (row);
	LuaInstance *li = LuaInstance::instance();
	li->interactive_add (LuaScriptInfo::EditorAction, row[_a_model.id]);
}

void
LuaScriptManager::del_action_btn_clicked ()
{
	TreeModel::Row row = *(_a_view.get_selection()->get_selected());
	assert (row);
	LuaInstance *li = LuaInstance::instance();
	if (!li->remove_lua_action (row[_a_model.id])) {
		// error
	}
}

void
LuaScriptManager::call_action_btn_clicked ()
{
	TreeModel::Row row = *(_a_view.get_selection()->get_selected());
	assert (row && row[_a_model.enabled]);
	LuaInstance *li = LuaInstance::instance();
	li->call_action (row[_a_model.id]);
}

void
LuaScriptManager::edit_action_btn_clicked ()
{
	TreeModel::Row row = *(_a_view.get_selection()->get_selected());
	assert (row);
	int id = row[_a_model.id];
	LuaInstance *li = LuaInstance::instance();
	std::string name, script;
	LuaScriptParamList args;
	if (!li->lua_action (id, name, script, args)) {
		return;
	}

	// TODO text-editor window, update script directly

	if (!LuaScripting::try_compile (script, args)) {
		// compilation failed, keep editing
		return;
	}

	if (li->set_lua_action (id, name, script, args)) {
		// OK
	} else {
		// load failed,  keep editing..
	}
	action_selection_changed ();
}

void
LuaScriptManager::set_action_script_name (int i, const std::string& name)
{
	typedef Gtk::TreeModel::Children type_children;
	type_children children = _a_store->children();
	for(type_children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		Gtk::TreeModel::Row row = *iter;
		if (row[_a_model.id] == i) {
			if (name.empty()) {
				row[_a_model.enabled] = false;
				row[_a_model.name] = _("Unset");
			} else {
				row[_a_model.enabled] = true;
				row[_a_model.name] = name;
			}
			break;
		}
	}
	action_selection_changed ();
}


void
LuaScriptManager::setup_callbacks ()
{
	LuaInstance *li = LuaInstance::instance();
	std::vector<PBD::ID> ids = li->lua_slots();
	for (std::vector<PBD::ID>::const_iterator i = ids.begin(); i != ids.end(); ++i) {
		std::string name;
		std::string script;
		ActionHook ah;
		LuaScriptParamList lsp;
		if (li->lua_slot (*i, name, script, ah, lsp)) {
			set_callback_script_name (*i, name, ah);
		}
	}
}

void
LuaScriptManager::callback_selection_changed ()
{
	TreeModel::Row row = *(_c_view.get_selection()->get_selected());
	if (row) {
		_c_del_button.set_sensitive (true);
	} else {
		_c_del_button.set_sensitive (false);
	}
}

void
LuaScriptManager::add_callback_btn_clicked ()
{
	LuaInstance *li = LuaInstance::instance();
	li->interactive_add (LuaScriptInfo::EditorHook, -1);
}

void
LuaScriptManager::del_callback_btn_clicked ()
{
	TreeModel::Row row = *(_c_view.get_selection()->get_selected());
	assert (row);
	LuaInstance *li = LuaInstance::instance();
	if (!li->unregister_lua_slot (row[_c_model.id])) {
		// error
	}
}

void
LuaScriptManager::set_callback_script_name (PBD::ID id, const std::string& name, const ActionHook& ah)
{
	if (name.empty()) {
		typedef Gtk::TreeModel::Children type_children;
		type_children children = _c_store->children();
		for(type_children::iterator iter = children.begin(); iter != children.end(); ++iter) {
			Gtk::TreeModel::Row row = *iter;
			PBD::ID i = row[_c_model.id];
			if (i == id) {
				_c_store->erase (iter);
				break;
			}
		}
	} else {
		TreeModel::Row r = *_c_store->append ();
		r[_c_model.id] = id;
		r[_c_model.name] = name;
		string sig;
		for (uint32_t i = 0; i < LuaSignal::LAST_SIGNAL; ++i) {
			if (ah[i]) {
				if (!sig.empty()) sig += ", ";
				sig += enum2str (LuaSignal::LuaSignal (i));
			}
		}
		r[_c_model.signals] = sig;
	}
	callback_selection_changed ();
}
