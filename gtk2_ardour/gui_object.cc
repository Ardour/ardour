/*
    Copyright (C) 2011 Paul Davis

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

#include <iostream>
#include <sstream>

#include "gui_object.h"
#include "i18n.h"

using std::string;

const string GUIObjectState::xml_node_name (X_("GUIObjectState"));

GUIObjectState::GUIObjectState ()
	: _state (X_("GUIObjectState"))
{

}

XMLNode *
GUIObjectState::get_node (const XMLNode* parent, const string& id)
{
	XMLNodeList const & children = parent->children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() != X_("Object")) {
			continue;
		}

		XMLProperty* p = (*i)->property (X_("id"));
		if (p && p->value() == id) {
			return *i;
		}
	}

	return 0;
}

XMLNode *
GUIObjectState::get_or_add_node (XMLNode* parent, const string& id)
{
	XMLNode* child = get_node (parent, id);
	if (!child) {
		child = new XMLNode (X_("Object"));
		child->add_property (X_("id"), id);
		parent->add_child_nocopy (*child);
	}

	return child;
}

XMLNode *
GUIObjectState::get_or_add_node (const string& id)
{
	return get_or_add_node (&_state, id);
}

/** Get a string from our state.
 *  @param id property of Object node to look for.
 *  @param prop_name name of the Object property to return.
 *  @param empty if non-0, filled in with true if the property is currently non-existant, otherwise false.
 *  @return value of property `prop_name', or empty.
 */

string 
GUIObjectState::get_string (const string& id, const string& prop_name, bool* empty)
{
	XMLNode* child = get_node (&_state, id);

	if (!child) {
		if (empty) {
			*empty = true;
		}
		return string ();
	}

	XMLProperty* p = child->property (prop_name);
	if (!p) {
		if (empty) {
			*empty = true;
		}
		return string ();
	}

	if (empty) {
		*empty = false;
	}

	return p->value ();
}

XMLNode&
GUIObjectState::get_state () const
{
	return *new XMLNode (_state);
}

int
GUIObjectState::set_state (const XMLNode& node)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	_state = node;
	return 0;
}

void
GUIObjectState::load (const XMLNode& node)
{
	(void) set_state (node);
}

std::list<string>
GUIObjectState::all_ids () const
{
	std::list<string> ids;
	XMLNodeList const & children = _state.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() != X_("Object")) {
			continue;
		}

		XMLProperty* p = (*i)->property (X_("id"));
		if (p) {
			ids.push_back (p->value ());
		}
	}

	return ids;
}

	
