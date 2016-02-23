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
#ifndef __ardour_luawindow_h__
#define __ardour_luawindow_h__

#include <glibmm/thread.h>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "lua/luastate.h"

class LuaWindow :
	public Gtk::Window,
	public PBD::ScopedConnectionList,
	public ARDOUR::SessionHandlePtr,
	public Gtkmm2ext::VisibilityTracker
{
  public:
	static LuaWindow* instance();
	~LuaWindow();

	void show_window ();
	bool hide_window (GdkEventAny *ev);

	void set_session (ARDOUR::Session* s);

  private:
	LuaWindow ();
	static LuaWindow* _instance;

	bool _visible;
	Gtk::VBox global_vpacker;

	void session_going_away ();
	void update_title ();

	Gtk::Entry entry;
	Gtk::TextView outtext;
	Gtk::ScrolledWindow scrollwin;

	void append_text (std::string s);
	void scroll_to_bottom ();
	void clear_output ();

	void entry_activated ();

	LuaState lua;
};

#endif
