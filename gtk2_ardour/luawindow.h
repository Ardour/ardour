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
#ifndef __ardour_luawindow_h__
#define __ardour_luawindow_h__

#include <glibmm/thread.h>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/textview.h>
#include <gtkmm/window.h>

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/luascripting.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "lua/luastate.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

class LuaWindow :
	public ArdourWindow,
	public PBD::ScopedConnectionList
{
public:
	static LuaWindow* instance();
	~LuaWindow();

	void show_window ();
	bool hide_window (GdkEventAny *ev);
	void edit_script (const std::string&, const std::string&);

	void set_session (ARDOUR::Session* s);

	typedef enum {
		Buffer_NOFLAG     = 0x00,
		Buffer_Valid      = 0x01, ///< script is loaded
		Buffer_HasFile    = 0x02,
		Buffer_ReadOnly   = 0x04,
		Buffer_Dirty      = 0x08,
		Buffer_Scratch    = 0x10,
	} BufferFlags;

	class ScriptBuffer
	{
	public:
		ScriptBuffer (const std::string&);
		ScriptBuffer (ARDOUR::LuaScriptInfoPtr);
		//ScriptBuffer (const ScriptBuffer& other);
		~ScriptBuffer ();

		bool load ();

		std::string script;
		std::string name;
		std::string path;
		BufferFlags flags;
		ARDOUR::LuaScriptInfo::ScriptType type;
	};

private:
	LuaWindow ();
	static LuaWindow* _instance;

	LuaState *lua;
	bool _visible;

	Gtk::Menu* _menu_scratch;
	Gtk::Menu* _menu_snippet;
	Gtk::Menu* _menu_actions;

	sigc::connection _script_changed_connection;

	Gtk::TextView entry;
	Gtk::TextView outtext;
	Gtk::ScrolledWindow scrollout;

	ArdourWidgets::ArdourButton _btn_run;
	ArdourWidgets::ArdourButton _btn_clear;
	ArdourWidgets::ArdourButton _btn_open;
	ArdourWidgets::ArdourButton _btn_save;
	ArdourWidgets::ArdourButton _btn_delete;
	ArdourWidgets::ArdourButton _btn_revert;

	ArdourWidgets::ArdourDropdown script_select;

	typedef boost::shared_ptr<ScriptBuffer> ScriptBufferPtr;
	typedef std::vector<ScriptBufferPtr> ScriptBufferList;

	ScriptBufferList script_buffers;
	ScriptBufferPtr _current_buffer;

	void session_going_away ();
	void update_title ();
	void reinit_lua ();

	void setup_buffers ();
	void refresh_scriptlist ();
	void rebuild_menu ();
	uint32_t count_scratch_buffers () const;

	void script_changed ();
	void script_selection_changed (ScriptBufferPtr n, bool force = false);
	void update_gui_state ();

	void append_text (std::string s);
	void scroll_to_bottom ();
	void clear_output ();

	void run_script ();

	void new_script ();
	void delete_script ();
	void revert_script ();
	void import_script ();
	void save_script ();
};


#endif
