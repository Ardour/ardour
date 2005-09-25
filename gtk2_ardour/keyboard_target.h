/*
    Copyright (C) 2001 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_keyboard_target_h__
#define __ardour_keyboard_target_h__

#include <map>
#include <string>
#include <sigc++/signal_system.h>
#include <gdk/gdk.h>
#include <gtk--/window.h>
#include <pbd/xml++.h>

#include "keyboard.h"

using std::map;
using std::string;

class KeyboardTarget 
{
  public:
	KeyboardTarget(Gtk::Window& w, string name);
	virtual ~KeyboardTarget();

	SigC::Signal0<void> Hiding;
	SigC::Signal0<void> GoingAway;

	typedef SigC::Slot0<void> KeyAction;

	string name() const { return _name; }

	void key_press_event (GdkEventKey *, Keyboard::State&, bool& handled);
	void key_release_event (GdkEventKey *, Keyboard::State&);

	int add_binding (string keys, string name);
	string get_binding (string name); /* returns keys bound to name */

	XMLNode& get_binding_state () const;
	int set_binding_state (const XMLNode&);

	static int32_t add_action (string, KeyAction);
	static int32_t find_action (string, KeyAction&);
	static int32_t remove_action (string);
	static void show_all_actions();

	Gtk::Window& window() const { return _window; }
	
  protected:
	typedef map<Keyboard::State,KeyAction> KeyMap;
	typedef map<string,string> BindingMap;

	KeyMap     keymap;
	BindingMap bindings;

  private:
	typedef map<string,KeyAction> ActionMap; 
	static ActionMap actions;
	string _name;
	Gtk::Window& _window;

	int load_bindings (const XMLNode&);
};

#endif /* __ardour_keyboard_target_h__ */

