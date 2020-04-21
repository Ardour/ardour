/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/messagedialog.h>

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/session.h"

#include "LuaBridge/LuaBridge.h"

#include "ardour_ui.h"
#include "lua_script_manager.h"
#include "luawindow.h"
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
	, _s_add_button (_("Load"))
	, _s_del_button (_("Remove"))
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

	Frame* f;
	Gtk::Label* doc;

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

	f = manage (new Frame (_("Description")));
	doc = manage (new Label (
				_("Action Scripts are user initiated actions (menu, shortcuts, toolbar-button) for batch processing or customized tasks.")
				));
	doc->set_padding (5, 5);
	doc->set_line_wrap();
	f->add (*doc);

	Gtk::ScrolledWindow *scroller = manage (new Gtk::ScrolledWindow());
	scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	scroller->add (_a_view);
	Gtk::VBox *vbox = manage (new VBox());
	vbox->pack_start (*scroller, true, true);
	vbox->pack_end (*edit_box, false, false);
	vbox->pack_end (*f, false, false);
	vbox->show_all ();

	pages.pages ().push_back (Notebook_Helpers::TabElem (*vbox, _("Action Scripts")));

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

	f = manage (new Frame (_("Description")));
	doc = manage (new Label (
				_("Lua action hooks are event-triggered callbacks for the Editor/Mixer GUI. Once a script is registered it is automatically triggered by events to perform some task.")
				));
	doc->set_padding (5, 5);
	doc->set_line_wrap();
	f->add (*doc);

	scroller = manage (new Gtk::ScrolledWindow());
	scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	scroller->add (_c_view);
	vbox = manage (new VBox());
	vbox->pack_start (*scroller, true, true);
	vbox->pack_end (*edit_box, false, false);
	vbox->pack_end (*f, false, false);
	vbox->show_all ();

	pages.pages ().push_back (Notebook_Helpers::TabElem (*vbox, _("Action Hooks")));

	/* session script page */

	_s_store = ListStore::create (_s_model);
	_s_view.set_model (_s_store);
	_s_view.append_column (_("Name"), _s_model.name);
	_s_view.get_column(0)->set_resizable (true);
	_s_view.get_column(0)->set_expand (true);

	edit_box = manage (new Gtk::HBox);
	edit_box->set_spacing(3);
	edit_box->pack_start (_s_add_button, true, true);
	edit_box->pack_start (_s_del_button, true, true);

	_s_add_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::add_sess_btn_clicked));
	_s_del_button.signal_clicked().connect (sigc::mem_fun(*this, &LuaScriptManager::del_sess_btn_clicked));
	_s_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &LuaScriptManager::session_script_selection_changed));

	f = manage (new Frame (_("Description")));
	doc = manage (new Label (
				_("Lua session scripts are loaded into processing engine and run in realtime. They are called periodically at the start of every audio cycle in the realtime process context before any processing takes place.")
				));
	doc->set_padding (5, 5);
	doc->set_line_wrap();
	f->add (*doc);

	scroller = manage (new Gtk::ScrolledWindow());
	scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	scroller->add (_s_view);
	vbox = manage (new VBox());
	vbox->pack_start (*scroller, true, true);
	vbox->pack_end (*edit_box, false, false);
	vbox->pack_end (*f, false, false);
	vbox->show_all ();

	pages.pages ().push_back (Notebook_Helpers::TabElem (*vbox, _("Session Scripts")));

	/* global layout */

	add (pages);
	pages.show();

	setup_actions ();
	setup_callbacks ();
	setup_session_scripts ();

	action_selection_changed ();
	callback_selection_changed ();
	session_script_selection_changed ();
}

void
LuaScriptManager::set_session (ARDOUR::Session *s)
{
	ArdourWindow::set_session (s);
	setup_session_scripts ();
	if (!_session) {
		return;
	}

	_session->LuaScriptsChanged.connect (_session_script_connection,  invalidator (*this), boost::bind (&LuaScriptManager::setup_session_scripts, this), gui_context());
	setup_session_scripts ();
}

void
LuaScriptManager::session_going_away ()
{
	ArdourWindow::session_going_away ();
	_session_script_connection.disconnect ();
	hide_all();
}

void
LuaScriptManager::setup_actions ()
{
	LuaInstance *li = LuaInstance::instance();
	for (int i = 0; i < MAX_LUA_ACTION_SCRIPTS; ++i) {
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
		_a_edit_button.set_sensitive (true);
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
	LuaWindow::instance()->edit_script (name, script);
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


void
LuaScriptManager::setup_session_scripts ()
{
	_s_store->clear ();
	if (!_session) {
		return;
	}
	std::vector<std::string> reg = _session->registered_lua_functions ();
	for (std::vector<string>::const_iterator i = reg.begin(); i != reg.end(); ++i) {
		TreeModel::Row r = *_s_store->append ();
		r[_s_model.name] = *i;
	}
	session_script_selection_changed ();
}

void
LuaScriptManager::session_script_selection_changed ()
{
	if (!_session) {
		_s_del_button.set_sensitive (false);
		_s_add_button.set_sensitive (false);
		return;
	}
	TreeModel::Row row = *(_s_view.get_selection()->get_selected());
	if (row) {
		_s_del_button.set_sensitive (true);
	} else {
		_s_del_button.set_sensitive (false);
	}
	_s_add_button.set_sensitive (true);
}

void
LuaScriptManager::add_sess_btn_clicked ()
{
	if (!_session) {
		return;
	}
	LuaInstance *li = LuaInstance::instance();
	li->interactive_add (LuaScriptInfo::Session, -1);
}

void
LuaScriptManager::del_sess_btn_clicked ()
{
	assert (_session);
	TreeModel::Row row = *(_s_view.get_selection()->get_selected());
	const std::string& name = row[_s_model.name];
	try {
		_session->unregister_lua_function (name);
	} catch (luabridge::LuaException const& e) {
		string msg = string_compose (_("Session script '%1' removal failed: %2"), name, e.what ());
		MessageDialog am (msg);
		am.run ();
	}
}
