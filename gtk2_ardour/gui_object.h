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

#ifndef __gtk_ardour_gui_object_h__
#define __gtk_ardour_gui_object_h__

#include <map>
#include <string>

#include <boost/variant.hpp>

#include "pbd/xml++.h"
#include "pbd/id.h"

class GUIObjectState
{
public:
	GUIObjectState ();

	static const std::string xml_node_name;
	void load (const XMLNode&);

	int set_state (const XMLNode&);
	XMLNode& get_state () const;

	/** Get a string from our state.
	 *  @param id property of Object node to look for.
	 *  @param prop_name name of the Object property to return.
	 *  @param empty if non-0, filled in with true if the property is currently non-existant, otherwise false.
	 *  @return value of property `prop_name', or empty.
	 */
	std::string get_string (const std::string& id, const std::string& prop_name, bool* empty = 0);

	template<typename T> void set_property (const std::string& id, const std::string& prop_name, const T& val) {
		XMLNode* child = get_or_add_node (id);
		child->set_property (prop_name.c_str(), val);
	}

	/** Remove node with provided id.
	 *  @param id property of Object node to look for.
	 */
	void remove_node (const std::string& id);
	std::list<std::string> all_ids () const;

	XMLNode* get_or_add_node (const std::string &);

	static XMLNode* get_node (const XMLNode *, const std::string &);
	static XMLNode* get_or_add_node (XMLNode *, const std::string &);

  private:
	// no copy construction. object_map saves pointers to _state XMLNodes
	// use set_state(get_state())
	GUIObjectState (const GUIObjectState& other);

	XMLNode _state;
	// ideally we'd use a O(1) hash table here,
	// but O(log(N)) is fine already.
	std::map <std::string, XMLNode*> object_map;
};

#endif /* __gtk_ardour_gui_object_h__ */
