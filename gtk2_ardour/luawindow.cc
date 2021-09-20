/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "pbd/gstdio_compat.h"
#include <glibmm/fileutils.h>
#include <gtkmm/messagedialog.h>

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/md5.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/pane.h"
#include "widgets/tooltips.h"

#include "ardour/filesystem_paths.h"
#include "ardour/luabindings.h"
#include "LuaBridge/LuaBridge.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "luainstance.h"
#include "luawindow.h"
#include "public_editor.h"
#include "utils.h"
#include "ui_config.h"
#include "utils_videotl.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;


inline LuaWindow::BufferFlags operator| (const LuaWindow::BufferFlags& a, const LuaWindow::BufferFlags& b) {
	return static_cast<LuaWindow::BufferFlags> (static_cast <int>(a) | static_cast<int> (b));
}

inline LuaWindow::BufferFlags operator|= (LuaWindow::BufferFlags& a, const LuaWindow::BufferFlags& b) {
	return a = static_cast<LuaWindow::BufferFlags> (static_cast <int>(a) | static_cast<int> (b));
}

inline LuaWindow::BufferFlags operator&= (LuaWindow::BufferFlags& a, const LuaWindow::BufferFlags& b) {
	return a = static_cast<LuaWindow::BufferFlags> (static_cast <int>(a) & static_cast<int> (b));
}

LuaWindow* LuaWindow::_instance = 0;

LuaWindow*
LuaWindow::instance ()
{
	if (!_instance) {
		_instance  = new LuaWindow;
	}

	return _instance;
}

LuaWindow::LuaWindow ()
	: ArdourWindow ("Lua")
	, lua (0)
	, _visible (false)
	, _menu_scratch (0)
	, _menu_snippet (0)
	, _menu_actions (0)
	, _btn_run (_("Run"))
	, _btn_clear (_("Clear Output"))
	, _btn_open (_("Import"))
	, _btn_save (_("Save"))
	, _btn_delete (_("Delete"))
	, _btn_revert (_("Revert"))
	, _current_buffer ()
{
	set_name ("Lua");

	reinit_lua ();
	update_title ();
	set_wmclass (X_("ardour_lua"), PROGRAM_NAME);

	script_select.disable_scrolling ();

	set_border_width (0);

	outtext.set_editable (false);
	outtext.set_wrap_mode (Gtk::WRAP_WORD);
	outtext.set_cursor_visible (false);

	signal_delete_event().connect (sigc::mem_fun (*this, &LuaWindow::hide_window));
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

	_btn_run.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::run_script));
	_btn_clear.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::clear_output));
	_btn_open.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::import_script));
	_btn_save.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::save_script));
	_btn_delete.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::delete_script));
	_btn_revert.signal_clicked.connect (sigc::mem_fun(*this, &LuaWindow::revert_script));

	_btn_open.set_sensitive (false); // TODO
	_btn_save.set_sensitive (false);
	_btn_delete.set_sensitive (false);
	_btn_revert.set_sensitive (false);

	// layout

	Gtk::ScrolledWindow *scrollin = manage (new Gtk::ScrolledWindow);
	scrollin->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scrollin->add (entry);
	scrollout.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
	scrollout.add (outtext);

	entry.set_name ("ArdourLuaEntry");
	outtext.set_name ("ArdourLuaEntry");

	Gtk::HBox *hbox = manage (new HBox());

	hbox->pack_start (_btn_run, false, false, 2);
	hbox->pack_start (_btn_clear, false, false, 2);
	hbox->pack_start (_btn_open, false, false, 2);
	hbox->pack_start (_btn_save, false, false, 2);
	hbox->pack_start (_btn_delete, false, false, 2);
	hbox->pack_start (_btn_revert, false, false, 2);
	hbox->pack_start (script_select, false, false, 2);

	Gtk::VBox *vbox = manage (new VBox());
	vbox->pack_start (*scrollin, true, true, 0);
	vbox->pack_start (*hbox, false, false, 2);

	ArdourWidgets::VPane *vpane = manage (new ArdourWidgets::VPane ());
	vpane->add (*vbox);
	vpane->add (scrollout);
	vpane->set_divider (0, 0.75);

	vpane->show_all ();
	add (*vpane);
	set_size_request (640, 480); // XXX
	ArdourWidgets::set_tooltip (script_select, _("Select Editor Buffer"));

	setup_buffers ();
	LuaScripting::instance().scripts_changed.connect (*this, invalidator (*this), boost::bind (&LuaWindow::refresh_scriptlist, this), gui_context());

	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	_script_changed_connection = tb->signal_changed().connect (sigc::mem_fun(*this, &LuaWindow::script_changed));
}

LuaWindow::~LuaWindow ()
{
	delete lua;
}

void
LuaWindow::show_window ()
{
	present();
	_visible = true;
}

bool
LuaWindow::hide_window (GdkEventAny *ev)
{
	if (!_visible) return 0;
	_visible = false;
	return ARDOUR_UI_UTILS::just_hide_it (ev, static_cast<Gtk::Window *>(this));
}

void LuaWindow::reinit_lua ()
{
	ENSURE_GUI_THREAD (*this, &LuaWindow::session_going_away);
	delete lua;
	lua = new LuaState();
	lua->Print.connect (sigc::mem_fun (*this, &LuaWindow::append_text));
	lua->sandbox (false);

	lua_State* L = lua->getState();
	LuaInstance::register_classes (L);
	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
}

void LuaWindow::set_session (Session* s)
{
	if (!s) {
		return;
	}
	/* only call SessionHandlePtr::set_session if session is not NULL,
	 * otherwise LuaWindow::session_going_away will never be invoked.
	 */
	ArdourWindow::set_session (s);

	update_title ();
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&LuaWindow::update_title, this), gui_context());

	lua_State* L = lua->getState();
	LuaBindings::set_session (L, _session);
}

void
LuaWindow::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaWindow::session_going_away);
	reinit_lua (); // drop state (all variables, session references)

	ArdourWindow::session_going_away ();
	_session = 0;
	update_title ();

	lua_State* L = lua->getState();
	LuaBindings::set_session (L, _session);
}

void
LuaWindow::update_title ()
{
	if (_session) {
		string n;

		if (_session->snap_name() != _session->name()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Lua");
		title += Glib::get_application_name ();
		set_title (title.get_string());

	} else {
		WindowTitle title (S_("Window|Lua"));
		title += Glib::get_application_name ();
		set_title (title.get_string());
	}
}

void
LuaWindow::scroll_to_bottom ()
{
	Gtk::Adjustment *adj;
	adj = scrollout.get_vadjustment();
	adj->set_value (MAX(0,(adj->get_upper() - adj->get_page_size())));
}

void
LuaWindow::run_script ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	std::string script = tb->get_text();
	const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
	if (bytecode.empty()) {
		// plain script or faulty script -- run directly
		try {
			lua->do_command ("function ardour () end");
			if (0 == lua->do_command (script)) {
				append_text ("> OK");
			}
		} catch (luabridge::LuaException const& e) {
			append_text (string_compose (_("LuaException: %1"), e.what()));
		} catch (Glib::Exception const& e) {
			append_text (string_compose (_("Glib Exception: %1"), e.what()));
		} catch (std::exception const& e) {
			append_text (string_compose (_("C++ Exception: %1"), e.what()));
		} catch (...) {
			append_text (string_compose (_("C++ Exception: %1"), "..."));
		}
	} else {
		// script with factory method
		try {
			lua_State* L = lua->getState();
			lua->do_command ("function ardour () end");

			LuaScriptParamList args = LuaScriptParams::script_params (script, "action_param", false);
			luabridge::LuaRef tbl_arg (luabridge::newTable(L));
			LuaScriptParams::params_to_ref (&tbl_arg, args);
			lua->do_command (script); // register "factory"
			luabridge::LuaRef lua_factory = luabridge::getGlobal (L, "factory");
			if (lua_factory.isFunction()) {
				lua_factory(tbl_arg)();
			}
			lua->do_command ("factory = nil;");
		} catch (luabridge::LuaException const& e) {
			append_text (string_compose (_("LuaException: %1"), e.what()));
		} catch (Glib::Exception const& e) {
			append_text (string_compose (_("Glib Exception: %1"), e.what()));
		} catch (std::exception const& e) {
			append_text (string_compose (_("C++ Exception: %1"), e.what()));
		} catch (...) {
			append_text (string_compose (_("C++ Exception: %1"), "..."));
		}
	}
	lua->collect_garbage ();
}

void
LuaWindow::append_text (std::string s)
{
	Glib::RefPtr<Gtk::TextBuffer> tb (outtext.get_buffer());
	tb->insert (tb->end(), s + "\n");
	scroll_to_bottom ();
	Gtkmm2ext::UI::instance()->flush_pending (0.05);
}

void
LuaWindow::clear_output ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (outtext.get_buffer());
	tb->set_text ("");
}

void
LuaWindow::edit_script (const std::string& name, const std::string& script)
{
	ScriptBuffer* sb = new LuaWindow::ScriptBuffer (name);
	sb->script = script;
	script_buffers.push_back (ScriptBufferPtr (sb));
	script_selection_changed (script_buffers.back ());
	refresh_scriptlist ();
	show_window ();
}

void
LuaWindow::new_script ()
{
	char buf[32];
	snprintf (buf, sizeof (buf), "#%d", count_scratch_buffers () + 1);
	script_buffers.push_back (ScriptBufferPtr (new LuaWindow::ScriptBuffer (buf)));
	script_selection_changed (script_buffers.back ());
	refresh_scriptlist ();
}

void
LuaWindow::delete_script ()
{
	assert ((_current_buffer->flags & Buffer_Scratch) || !(_current_buffer->flags & Buffer_ReadOnly));
	bool refresh = false;
	bool neednew = true;
	if (_current_buffer->flags & Buffer_HasFile) {
		if (0 == ::g_unlink (_current_buffer->path.c_str())) {
			append_text (X_("> ") + string_compose (_("Deleted %1"), _current_buffer->path));
			refresh = true;
		} else {
			append_text (X_("> ") + string_compose (_("Failed to delete %1"), _current_buffer->path));
		}
	}
	for (ScriptBufferList::iterator i = script_buffers.begin (); i != script_buffers.end (); ++i) {
		if ((*i) == _current_buffer) {
			script_buffers.erase (i);
			break;
		}
	}

	for (ScriptBufferList::const_iterator i = script_buffers.begin (); i != script_buffers.end (); ++i) {
		if ((*i)->flags & Buffer_Scratch) {
			script_selection_changed (*i);
			neednew = false;
		}
	}
	if (neednew) {
		new_script ();
	}
	if (refresh) {
		LuaScripting::instance ().refresh (true);
	}
}

void
LuaWindow::revert_script ()
{
	_current_buffer->flags &= BufferFlags(~Buffer_Valid);
	script_selection_changed (_current_buffer, true);
}

void
LuaWindow::import_script ()
{
	// TODO: dialog to select file or enter URL
	// TODO convert a few URL (eg. pastebin) to raw.
#if 0
	char *url = "http://pastebin.com/raw/3UMkZ6nV";
	char *rv = ArdourCurl::http_get (url, 0. true);
	if (rv) {
		new_script ();
		Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
		tb->set_text (rv);
		_current_buffer->flags &= BufferFlags(~Buffer_Dirty);
		update_gui_state ();
	}
	free (rv);
#endif
}

void
LuaWindow::save_script ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	std::string script = tb->get_text();
	std::string msg = "Unknown error";

	std::string path;
	LuaScriptInfoPtr lsi = LuaScripting::script_info (script);
	ScriptBuffer & sb (*_current_buffer);

	assert (sb.flags & Buffer_Dirty);

	// 1) check if it has a valid header and factory
	const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
	if (bytecode.empty()) {
		msg = _("Missing script header.\nThe script requires an '{ardour}' info table and a 'factory' function.");
		goto errorout;
	}

	if (!LuaScripting::try_compile (script, LuaScriptParams::script_params (script, "action_param", false))) {
		msg = _("Script fails to compile.");
		goto errorout;
	}

	// 2) check script name & type
	lsi = LuaScripting::script_info (script);
	if (!lsi) {
		msg = _("Invalid or missing script-name or script-type.");
		goto errorout;
	}

	if (lsi->type != LuaScriptInfo::Snippet && lsi->type != LuaScriptInfo::EditorAction) {
		msg = _("Invalid script-type.\nValid types are 'EditorAction' and 'Snippet'.");
		goto errorout;
	}

	// 3) if there's already a writable file,...
	if ((sb.flags & Buffer_HasFile) && !(sb.flags & Buffer_ReadOnly)) {
		try {
			Glib::file_set_contents (sb.path, script);
			sb.name = lsi->name;
			sb.flags &= BufferFlags(~Buffer_Dirty);
			update_gui_state (); // XXX here?
			append_text (X_("> ") + string_compose (_("Saved as %1"), sb.path));
			return; // OK
		} catch (Glib::FileError const& e) {
			msg = string_compose (_("Error saving file: %1"), e.what());
			goto errorout;
		}
	}

	// 4) check if the name is unique for the given type; locally at least
	if (true /*sb.flags & Buffer_HasFile*/) {
		LuaScriptList& lsl (LuaScripting::instance ().scripts (lsi->type));
		for (LuaScriptList::const_iterator s = lsl.begin(); s != lsl.end(); ++s) {
			if ((*s)->name == lsi->name) {
				msg = string_compose (_("Script with given name '%1' already exists.\nUse a different name in the descriptor."), lsi->name);
				goto errorout;
			}
		}
	}

	// 5) construct filename -- TODO ask user for name, ask to replace file.
	do {
		char tme[80];
		char buf[100];
		time_t t = time(0);
		struct tm * timeinfo = localtime (&t);
		strftime (tme, sizeof(tme), "%s", timeinfo);
		snprintf (buf, sizeof(buf), "%s%ld", tme, random ());
		MD5 md5;
		std::string fn = md5.digestString (buf);

		switch (lsi->type) {
			case LuaScriptInfo::EditorAction:
				fn = "a_" + fn;
				break;
			case LuaScriptInfo::Snippet:
				fn = "s_" + fn;
				break;
			default:
				break;
		}
		path = Glib::build_filename (LuaScripting::user_script_dir (), fn.substr(0, 11) + ".lua");
	} while (Glib::file_test (path, Glib::FILE_TEST_EXISTS));

	try {
		Glib::file_set_contents (path, script);
		sb.path = path;
		sb.name = lsi->name;
		sb.flags |= Buffer_HasFile;
		sb.flags &= BufferFlags(~Buffer_Dirty);
		sb.flags &= BufferFlags(~Buffer_ReadOnly);
		update_gui_state (); // XXX here? .refresh (true) may trigger this, too
		LuaScripting::instance().refresh (true);
		append_text (X_("> ") + string_compose (_("Saved as %1"), path));
		return; // OK
	} catch (Glib::FileError const& e) {
		msg = string_compose (_("Error saving file: %1"), e.what());
		goto errorout;
	}

errorout:
		MessageDialog am (msg);
		am.run ();
}

void
LuaWindow::setup_buffers ()
{
	if (script_buffers.size() > 0) {
		return;
	}
	script_buffers.push_back (ScriptBufferPtr (new LuaWindow::ScriptBuffer("#1")));
	_current_buffer = script_buffers.front();

	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	tb->set_text (_current_buffer->script);

	refresh_scriptlist ();
	update_gui_state ();
}

uint32_t
LuaWindow::count_scratch_buffers () const
{
	uint32_t n = 0;
	for (ScriptBufferList::const_iterator i = script_buffers.begin (); i != script_buffers.end (); ++i) {
		if ((*i)->flags & Buffer_Scratch) {
			++n;
		}
	}
	return n;
}

void
LuaWindow::refresh_scriptlist ()
{
	for (ScriptBufferList::iterator i = script_buffers.begin (); i != script_buffers.end ();) {
		if ((*i)->flags & Buffer_Scratch) {
			++i;
			continue;
		}
		i = script_buffers.erase (i);
	}
	LuaScriptList& lsa (LuaScripting::instance ().scripts (LuaScriptInfo::EditorAction));
	for (LuaScriptList::const_iterator s = lsa.begin(); s != lsa.end(); ++s) {
		script_buffers.push_back (ScriptBufferPtr (new LuaWindow::ScriptBuffer(*s)));
	}

	LuaScriptList& lss (LuaScripting::instance ().scripts (LuaScriptInfo::Snippet));
	for (LuaScriptList::const_iterator s = lss.begin(); s != lss.end(); ++s) {
		script_buffers.push_back (ScriptBufferPtr (new LuaWindow::ScriptBuffer(*s)));
	}
	rebuild_menu ();
}

void
LuaWindow::rebuild_menu ()
{
	using namespace Menu_Helpers;

	_menu_scratch = manage (new Menu);
	_menu_snippet = manage (new Menu);
	_menu_actions = manage (new Menu);

	MenuList& items_scratch (_menu_scratch->items());
	MenuList& items_snippet (_menu_snippet->items());
	MenuList& items_actions (_menu_actions->items());

	{
		Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(_("New"),
				sigc::mem_fun(*this, &LuaWindow::new_script));
		items_scratch.push_back(elem);
	}

	items_scratch.push_back(SeparatorElem());

	for (ScriptBufferList::const_iterator i = script_buffers.begin (); i != script_buffers.end (); ++i) {
		std::string name;
		if ((*i)->flags & Buffer_ReadOnly) {
			name = "[R] " + (*i)->name;
		} else {
			name = (*i)->name;
		}
		Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(name,
				sigc::bind(sigc::mem_fun(*this, &LuaWindow::script_selection_changed), (*i), false));

		if ((*i)->flags & Buffer_Scratch) {
			items_scratch.push_back(elem);
		}
		else if ((*i)->type == LuaScriptInfo::EditorAction) {
				items_actions.push_back(elem);
		}
		else if ((*i)->type == LuaScriptInfo::Snippet) {
				items_snippet.push_back(elem);
		}
	}

	script_select.clear_items ();
	script_select.AddMenuElem (Menu_Helpers::MenuElem ("Scratch", *_menu_scratch));
	script_select.AddMenuElem (Menu_Helpers::MenuElem ("Snippets", *_menu_snippet));
	script_select.AddMenuElem (Menu_Helpers::MenuElem ("Actions", *_menu_actions));
}

void
LuaWindow::script_selection_changed (ScriptBufferPtr n, bool force)
{
	if (n == _current_buffer && !force) {
		return;
	}

	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());

	if (_current_buffer->flags & Buffer_Valid) {
		_current_buffer->script = tb->get_text();
	}

	if (!(n->flags & Buffer_Valid)) {
		if (!n->load()) {
			append_text ("! Failed to load buffer.");
		}
	}

	if (n->flags & Buffer_Valid) {
		_current_buffer = n;
		_script_changed_connection.block ();
		tb->set_text (n->script);
		_script_changed_connection.unblock ();
	} else {
		append_text ("! Failed to switch buffer.");
	}
	update_gui_state ();
}

void
LuaWindow::update_gui_state ()
{
	const ScriptBuffer & sb (*_current_buffer);
	std::string name;
	if (sb.flags & Buffer_Scratch) {
		name = string_compose (_("Scratch Buffer %1"), sb.name);
	} else if (sb.type == LuaScriptInfo::EditorAction) {
		name = string_compose (_("Action: '%1'"), sb.name);
	} else if (sb.type == LuaScriptInfo::Snippet) {
		name = string_compose (_("Snippet: %1"), sb.name);
	} else {
		cerr << "Invalid Script type\n";
		assert (0);
		return;
	}
	if (sb.flags & Buffer_Dirty) {
		name += " *";
	}
	script_select.set_text(name);

	if (sb.flags & Buffer_ReadOnly) {
		_btn_save.set_text (_("Save as"));
	} else {
		_btn_save.set_text (_("Save"));
	}
	_btn_save.set_sensitive (sb.flags & Buffer_Dirty);
	_btn_delete.set_sensitive (sb.flags & Buffer_Scratch || ((sb.flags & (Buffer_ReadOnly | Buffer_HasFile)) == Buffer_HasFile));
	_btn_revert.set_sensitive ((sb.flags & Buffer_Dirty) && (sb.flags & Buffer_HasFile));
}

void
LuaWindow::script_changed () {
	if (_current_buffer->flags & Buffer_Dirty) {
		return;
	}
	_current_buffer->flags |= Buffer_Dirty;
	update_gui_state ();
}

LuaWindow::ScriptBuffer::ScriptBuffer (const std::string& n)
	: name (n)
	, flags (Buffer_Scratch | Buffer_Valid)
{
	script =
		"---- this header is (only) required to save the script\n"
		"-- ardour { [\"type\"] = \"Snippet\", name = \"\" }\n"
		"-- function factory () return function () -- -- end end\n";
}

LuaWindow::ScriptBuffer::ScriptBuffer (LuaScriptInfoPtr p)
	: name (p->name)
	, path (p->path)
	, flags (Buffer_HasFile)
	, type (p->type)
{
	if (!PBD::exists_and_writable (path)) {
		flags |= Buffer_ReadOnly;
	}
	if (path.find (user_config_directory ()) != 0) {
		// mark non-user scripts as read-only
		flags |= Buffer_ReadOnly;
	}
}

#if 0
LuaWindow::ScriptBuffer::ScriptBuffer (const ScriptBuffer& other)
	: script (other.script)
	, name (other.name)
	, path (other.path)
	, flags (other.flags)
	, type (other.type)
{
}
#endif

LuaWindow::ScriptBuffer::~ScriptBuffer ()
{
}

bool
LuaWindow::ScriptBuffer::load ()
{
	assert (!(flags & Buffer_Valid));
	if (!(flags & Buffer_HasFile)) return false;
	try {
		script = Glib::file_get_contents (path);
		flags |= Buffer_Valid;
		flags &= BufferFlags(~Buffer_Dirty);
	} catch (Glib::FileError const& e) {
		return false;
	}
	return true;
}
