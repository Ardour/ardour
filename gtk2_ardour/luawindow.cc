/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour_ui.h"
#include "gui_thread.h"
#include "luainstance.h"
#include "luawindow.h"
#include "public_editor.h"
#include "utils.h"

#include "ardour/luabindings.h"
#include "LuaBridge/LuaBridge.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;


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
	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*) this))
	, _visible (false)
{
	set_name ("Lua");

	update_title ();
	set_wmclass (X_("ardour_mixer"), PROGRAM_NAME);

	set_border_width (0);

	outtext.set_editable (false);
	outtext.set_wrap_mode (Gtk::WRAP_WORD);

	signal_delete_event().connect (sigc::mem_fun (*this, &LuaWindow::hide_window));
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

	scrollwin.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
	scrollwin.add (outtext);

	Gtk::Button *btn_clr = manage (new Button ("Clear"));
	btn_clr->signal_clicked().connect (sigc::mem_fun(*this, &LuaWindow::clear_output));

	Gtk::HBox *hbox = manage (new HBox());

	hbox->pack_start (entry, true, true, 2);
	hbox->pack_start (*btn_clr, false, false, 0);

	Gtk::VBox *vbox = manage (new VBox());
	vbox->pack_start (scrollwin, true, true, 0);
	vbox->pack_start (*hbox, false, false, 2);

	entry.signal_activate().connect (sigc::mem_fun (*this, &LuaWindow::entry_activated));

	lua.Print.connect (sigc::mem_fun (*this, &LuaWindow::append_text));

	vbox->show_all ();
	add (*vbox);
	set_size_request (640, 480); // XXX

	LuaInstance::register_classes (lua.getState());
	// TODO register some callback functions.

	lua_State* L = lua.getState();
	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
	// TODO
	// - allow to load files
	// - allow to run files directly
	// - history buffer
	// - multi-line input ??
}

LuaWindow::~LuaWindow ()
{
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
	return just_hide_it (ev, static_cast<Gtk::Window *>(this));
}

void LuaWindow::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!_session) {
		return;
	}

	update_title ();
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&LuaWindow::update_title, this), gui_context());

	// expose "Session" point directly
	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);
}

void
LuaWindow::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaWindow::session_going_away);
	lua.do_command ("collectgarbage();");
	//TODO: re-init lua-engine (drop all references) ??

	SessionHandlePtr::session_going_away ();
	_session = 0;
	update_title ();

	lua_State* L = lua.getState();
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
	adj = scrollwin.get_vadjustment();
	adj->set_value (MAX(0,(adj->get_upper() - adj->get_page_size())));
}

void
LuaWindow::entry_activated ()
{
	std::string cmd = entry.get_text();
	append_text ("> " + cmd);

	if (0 == lua.do_command (cmd)) {
		entry.set_text("");
	}
}

void
LuaWindow::append_text (std::string s)
{
	Glib::RefPtr<Gtk::TextBuffer> tb (outtext.get_buffer());
	tb->insert (tb->end(), s + "\n");
	scroll_to_bottom ();
}

void
LuaWindow::clear_output ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (outtext.get_buffer());
	tb->set_text ("");
}
