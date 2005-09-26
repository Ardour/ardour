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
#include <sigc++/signal.h>
#include <sigc++/slot.h>
#include <gdk/gdk.h>
#include <pbd/xml++.h>

#include "keyboard.h"

namespace Gtk {
	class Window;
}

class KeyboardTarget 
{
  public:
	KeyboardTarget(Gtk::Window& w, std::string name);
	virtual ~KeyboardTarget();

	sigc::signal<void> Hiding;
	sigc::signal<void> GoingAway;

	typedef sigc::slot<void> KeyAction;

	std::string name() const { return _name; }

	void key_press_event (GdkEventKey *, Keyboard::State&, bool& handled);
	void key_release_event (GdkEventKey *, Keyboard::State&);

	int add_binding (std::string keys, std::string name);
	std::string get_binding (std::string name); /* returns keys bound to name */

	XMLNode& get_binding_state () const;
	int set_binding_state (const XMLNode&);

	static int32_t add_action (std::string, KeyAction);
	static int32_t find_action (std::string, KeyAction&);
	static int32_t remove_action (std::string);
	static void show_all_actions();

	Gtk::Window& window() const { return _window; }
	
  protected:
	typedef std::map<Keyboard::State,KeyAction> KeyMap;
	typedef std::map<std::string,std::string> BindingMap;

	KeyMap     keymap;
	BindingMap bindings;

  private:
	typedef map<std::string,KeyAction> ActionMap; 
	static ActionMap actions;
	std::string _name;
	Gtk::Window& _window;

	int load_bindings (const XMLNode&);
};

#endif /* __ardour_keyboard_target_h__ */

