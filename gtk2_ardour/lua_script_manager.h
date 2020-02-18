/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef _gtk2_ardour_lua_script_manager_h_
#define _gtk2_ardour_lua_script_manager_h_

#include <gtkmm/button.h>
#include <gtkmm/notebook.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

#include "ardour/luascripting.h"

#include "ardour_window.h"
#include "luainstance.h"

class LuaScriptManager : public ArdourWindow
{
public:
	LuaScriptManager ();
	void set_session (ARDOUR::Session *);

protected:
	void session_going_away();

private:
	Gtk::Notebook pages;

	/* action scripts */
	void setup_actions ();
	void action_selection_changed ();
	void set_action_script_name (int, const std::string&);

	void set_action_btn_clicked ();
	void del_action_btn_clicked ();
	void edit_action_btn_clicked ();
	void call_action_btn_clicked ();

	class LuaActionScriptModelColumns : public Gtk::TreeModelColumnRecord
	{
		public:
			LuaActionScriptModelColumns ()
			{
				add (id);
				add (action);
				add (name);
				add (enabled);
			}

			Gtk::TreeModelColumn<int> id;
			Gtk::TreeModelColumn<std::string> action;
			Gtk::TreeModelColumn<std::string> name;
			Gtk::TreeModelColumn<bool> enabled;
	};

	Gtk::Button _a_set_button;
	Gtk::Button _a_del_button;
	Gtk::Button _a_edit_button;
	Gtk::Button _a_call_button;

	Glib::RefPtr<Gtk::ListStore> _a_store;
	LuaActionScriptModelColumns _a_model;
	Gtk::TreeView _a_view;

	/* action callback hooks */
	void setup_callbacks ();
	void callback_selection_changed ();
	void set_callback_script_name (PBD::ID, const std::string&, const ActionHook& ah);

	void add_callback_btn_clicked ();
	void del_callback_btn_clicked ();

	class LuaCallbackScriptModelColumns : public Gtk::TreeModelColumnRecord
	{
		public:
			LuaCallbackScriptModelColumns ()
			{
				add (id);
				add (name);
				add (signals);
			}

			Gtk::TreeModelColumn<PBD::ID> id;
			Gtk::TreeModelColumn<std::string> name;
			Gtk::TreeModelColumn<std::string> signals;
	};

	Glib::RefPtr<Gtk::ListStore> _c_store;
	LuaCallbackScriptModelColumns _c_model;
	Gtk::TreeView _c_view;

	Gtk::Button _c_add_button;
	Gtk::Button _c_del_button;

	/* Session scripts */
	void setup_session_scripts ();
	void session_script_selection_changed ();

	void add_sess_btn_clicked ();
	void del_sess_btn_clicked ();

	class LuaSessionScriptModelColumns : public Gtk::TreeModelColumnRecord
	{
		public:
			LuaSessionScriptModelColumns ()
			{
				add (name);
			}

			Gtk::TreeModelColumn<std::string> name;
	};

	Glib::RefPtr<Gtk::ListStore> _s_store;
	LuaCallbackScriptModelColumns _s_model;
	Gtk::TreeView _s_view;

	Gtk::Button _s_add_button;
	Gtk::Button _s_del_button;

	PBD::ScopedConnection _session_script_connection;
};

#endif /* _gtk2_ardour_lua_script_manager_h_ */
