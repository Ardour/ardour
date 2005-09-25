/*
    Copyright (C) 2001-2002 Paul Davis 

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

#include <gdk/gdkkeysyms.h>
#include <pbd/error.h>

#include "keyboard.h"
#include "keyboard_target.h"

#include "i18n.h"

using std::pair;

KeyboardTarget::ActionMap KeyboardTarget::actions;

KeyboardTarget::KeyboardTarget (Gtk::Window& win, string name)
	: _window (win)
{
	_name = name;
	Keyboard::the_keyboard().register_target (this);
}

KeyboardTarget::~KeyboardTarget ()
{
	GoingAway ();
}

void
KeyboardTarget::key_release_event (GdkEventKey *event, Keyboard::State& state)
{
	// relax
}

void
KeyboardTarget::key_press_event (GdkEventKey *event, Keyboard::State& state, bool& handled)
{
	KeyMap::iterator result;
	
	if ((result = keymap.find (state)) != keymap.end()) {
		(*result).second ();
		handled = true;
	}
}

int
KeyboardTarget::add_binding (string keystring, string action)
{
	KeyMap::iterator existing;
	Keyboard::State  state;
	KeyAction key_action = 0;

	state = Keyboard::translate_key_name (keystring);

	if (keystring.length() == 0) {
		error << _("KeyboardTarget: empty string passed to add_binding.")
		      << endmsg;
		return -1;
	}

	if (state.size() == 0) {
		error << compose(_("KeyboardTarget: no translation found for \"%1\""), keystring) << endmsg;
		return -1;
	}

	if (find_action (action, key_action)) {
		error << compose(_("KeyboardTarget: unknown action \"%1\""), action) << endmsg;
		return -1;
	}

	/* remove any existing binding */

	if ((existing = keymap.find (state)) != keymap.end()) {
		keymap.erase (existing);
	}
	
	keymap.insert (pair<Keyboard::State,KeyAction> (state, key_action));
	bindings.insert (pair<string,string> (keystring, action));
	return 0;
}

string
KeyboardTarget::get_binding (string name)
{
	BindingMap::iterator i;
	
	for (i = bindings.begin(); i != bindings.end(); ++i) {

		if (i->second == name) {

			/* convert keystring to GTK format */

			string str = i->first;
			string gtkstr;
			string::size_type p;

			while (1) {

				if ((p = str.find ('-')) == string::npos || (p == str.length() - 1)) {
					break;
				}

				gtkstr += '<';
				gtkstr += str.substr (0, p);
				gtkstr += '>';

				str = str.substr (p+1);

			}

			gtkstr += str;

			if (gtkstr.length() == 0) {
				return i->first;
			} 

			return gtkstr;
		}
	}
	return string ();
}

void
KeyboardTarget::show_all_actions ()
{
	ActionMap::iterator i;
	
	for (i = actions.begin(); i != actions.end(); ++i) {
		cout << i->first << endl;
	}
}

int
KeyboardTarget::add_action (string name, KeyAction action)
{
	pair<string,KeyAction> newpair;
	pair<ActionMap::iterator,bool> result;
	newpair.first = name;
	newpair.second = action;

	result = actions.insert (newpair);
	return result.second ? 0 : -1;
}

int
KeyboardTarget::find_action (string name, KeyAction& action)
{
	map<string,KeyAction>::iterator i;

	if ((i = actions.find (name)) != actions.end()) {
		action = i->second;
		return 0;
	} else {
		return -1;
	}
}

int
KeyboardTarget::remove_action (string name)
{
	map<string,KeyAction>::iterator i;

	if ((i = actions.find (name)) != actions.end()) {
		actions.erase (i);
		return 0;
	} else {
		return -1;
	}
}

XMLNode&
KeyboardTarget::get_binding_state () const
{
	XMLNode *node = new XMLNode ("context");
	BindingMap::const_iterator i;

	node->add_property ("name", _name);
       
	for (i = bindings.begin(); i != bindings.end(); ++i) {
		XMLNode *child;

		child = new XMLNode ("binding");
		child->add_property ("keys", i->first);
		child->add_property ("action", i->second);
		node->add_child_nocopy (*child);
	}
	
	return *node;
}
	
int
KeyboardTarget::set_binding_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;

	bindings.clear ();
	keymap.clear ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "context") {
			XMLProperty *prop;
			
			if ((prop = child_node->property ("name")) != 0) {
				if (prop->value() == _name) {
					return load_bindings (*child_node);
				}
			}
		}
	}

	return 0;
}

int
KeyboardTarget::load_bindings (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		XMLProperty *keys;
		XMLProperty *action;
		
		keys = (*niter)->property ("keys");
		action = (*niter)->property ("action");

		if (!keys || !action) {
			error << _("misformed binding node - ignored") << endmsg;
			continue;
		}

		add_binding (keys->value(), action->value());
			
	}

	return 0;
}

