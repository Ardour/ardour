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
#include <iomanip>
#include <sstream>

#include <boost/variant/static_visitor.hpp>

#include "gui_object.h"
#include "i18n.h"

using std::string;

const string GUIObjectState::xml_node_name (X_("GUIObjectState"));

GUIObjectState::~GUIObjectState ()
{
	clear_maps ();
}

void
GUIObjectState::clear_maps ()
{
	_property_maps.clear ();
}

class gos_string_vistor : public boost::static_visitor<> {
  public:
    gos_string_vistor (std::ostream& o) 
	    : stream (o) {}
	    
    void operator() (const int64_t& i) {
	    stream << i;
    }

    void operator() (const std::string& s) {
	    stream << s;
    }

  private:
    std::ostream& stream;
};

std::string 
GUIObjectState::get_string (const std::string& id, const std::string& prop_name, bool* empty)
{
	StringPropertyMap::iterator i = _property_maps.find (id);
	
	if (i == _property_maps.end()) {
		if (empty) {
			*empty = true;
		}
		return string();
	}
	
	const PropertyMap& pmap (i->second);
	PropertyMap::const_iterator p = pmap.find (prop_name);
	
	if (p == pmap.end()) {
		if (empty) {
			*empty = true;
		}
		return string();
	}
	
	std::stringstream ss;
	gos_string_vistor gsv (ss);

	boost::apply_visitor (gsv, p->second);

	if (empty) {
		*empty = false;
	}
	
	return ss.str ();
}

XMLNode&
GUIObjectState::get_state () const
{
	XMLNode* root = new XMLNode (xml_node_name);
	
	for (StringPropertyMap::const_iterator i = _property_maps.begin(); i != _property_maps.end(); ++i) {

		const PropertyMap& pmap (i->second);
		XMLNode* id_node = new XMLNode (X_("Object"));
		
		id_node->add_property ("id", i->first);
		
		for (PropertyMap::const_iterator p = pmap.begin(); p != pmap.end(); ++p) {
			std::stringstream ss;
			gos_string_vistor gsv (ss);
			boost::apply_visitor (gsv, p->second);
			id_node->add_property (p->first.c_str(), ss.str());
		}
		
		root->add_child_nocopy (*id_node);
	}


	return *root;
}

int
GUIObjectState::set_state (const XMLNode& node)
{
	if (node.name() != xml_node_name) {
		return -1;
	}
	
	clear_maps ();
	
	for (XMLNodeList::const_iterator i = node.children().begin(); i != node.children().end(); ++i) {
		if ((*i)->name() == X_("Object")) {

			XMLNode* child = (*i);
			const XMLProperty* idp = child->property (X_("id"));

			if (!idp) {
				continue;
			}

			string id (idp->value());
			
			for (XMLPropertyList::const_iterator p = child->properties().begin(); p != child->properties().end(); ++p) {
				/* note that this always sets the property with
				   a string value, and so is not equivalent to
				   a call made by the program that passed a
				   scalar.
				*/
				if ((*p)->name() != X_("id")) {
					set (id, (*p)->name(), (*p)->value());
				}
			}
		}
	}

	return 0;
}


void
GUIObjectState::load (const XMLNode& node)
{
	(void) set_state (node);
}

GUIObjectState&
GUIObjectState::operator= (const GUIObjectState& other)
{
	_property_maps = other._property_maps;

	return *this;
}

/** @return begin iterator into our StringPropertyMap */
GUIObjectState::StringPropertyMap::const_iterator
GUIObjectState::begin () const
{
	return _property_maps.begin ();
}

/** @return end iterator into our StringPropertyMap */
GUIObjectState::StringPropertyMap::const_iterator
GUIObjectState::end () const
{
	return _property_maps.end ();
}

