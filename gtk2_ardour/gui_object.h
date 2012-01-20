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

#include "i18n.h"

class GUIObjectState
{
public:
	GUIObjectState ();
	
	XMLNode& get_state () const;
	int set_state (const XMLNode&);

	static const std::string xml_node_name;
	void load (const XMLNode&);

	GUIObjectState& operator= (const GUIObjectState& other);

	std::string get_string (const std::string& id, const std::string& prop_name, bool* empty = 0);

	template<typename T> void set (const std::string& id, const std::string& prop_name, const T& val) {
		XMLNode* child = find_node (id);
		if (!child) {
			child = new XMLNode (X_("Object"));
			child->add_property (X_("id"), id);
			_state.add_child_nocopy (*child);
		}
		
		std::stringstream s;
		s << val;
		child->add_property (prop_name.c_str(), s.str());
	}

	std::list<std::string> all_ids () const;
	
  private:
	XMLNode* find_node (const std::string &) const;

	XMLNode _state;
};


#endif /* __gtk_ardour_gui_object_h__ */
