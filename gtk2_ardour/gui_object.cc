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
#include "pbd/i18n.h"

using std::string;

/*static*/ XMLNode *
GUIObjectState::get_node (const XMLNode* parent, const string& id)
{
	XMLNodeList const & children = parent->children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() != X_("Object")) {
			continue;
		}
		if ((*i)->has_property_with_value(X_("id"), id)) {
			return *i;
		}
	}
	return 0;
}

/*static*/ XMLNode *
GUIObjectState::get_or_add_node (XMLNode* parent, const string& id)
{
	XMLNode* child = get_node (parent, id);
	if (!child) {
		child = new XMLNode (X_("Object"));
		child->set_property (X_("id"), id);
		parent->add_child_nocopy (*child);
	}
	return child;
}


const string GUIObjectState::xml_node_name (X_("GUIObjectState"));

GUIObjectState::GUIObjectState ()
	: _state (X_("GUIObjectState"))
{
}

XMLNode *
GUIObjectState::get_or_add_node (const string& id)
{
	std::map <std::string, XMLNode*>::iterator i = object_map.find (id);
	if (i != object_map.end()) {
		return i->second;
	}
	//assert (get_node (&_state, id) == 0); // XXX performance penalty due to get_node()
	XMLNode* child = new XMLNode (X_("Object"));
	child->set_property (X_("id"), id);
	_state.add_child_nocopy (*child);
	object_map[id] = child;
	return child;
}

void
GUIObjectState::remove_node (const std::string& id)
{
	object_map.erase (id);
	_state.remove_nodes_and_delete(X_("id"), id );
}

string
GUIObjectState::get_string (const string& id, const string& prop_name, bool* empty)
{
	std::map <std::string, XMLNode*>::const_iterator i = object_map.find (id);
	if (i == object_map.end()) {
		//assert (get_node (&_state, id) == 0); // XXX performance penalty due to get_node()
		if (empty) {
			*empty = true;
		}
		return string ();
	}
	//assert (get_node (&_state, id) == i->second); // XXX performance penalty due to get_node()

	XMLProperty const * p (i->second->property (prop_name));
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

	object_map.clear ();
	_state = node;

	XMLNodeList const & children (_state.children ());
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() != X_("Object")) {
			continue;
		}
		XMLProperty const * prop = (*i)->property (X_("id"));
		if (!prop) {
			continue;
		}
		object_map[prop->value ()] = *i;
	}
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
	for (std::map <std::string, XMLNode*>::const_iterator i = object_map.begin ();
			i != object_map.end (); ++i) {
		ids.push_back (i->first);
	}
	return ids;
}
