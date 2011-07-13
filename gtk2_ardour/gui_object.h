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

class GUIObjectState {
  private:
	typedef boost::variant<int64_t,std::string> Variant;
	typedef std::map<std::string,Variant> PropertyMap;

  public:
	typedef std::map<std::string,PropertyMap> StringPropertyMap;
	
	~GUIObjectState();

	StringPropertyMap::const_iterator begin () const;
	StringPropertyMap::const_iterator end () const;

	XMLNode& get_state () const;
	int set_state (const XMLNode&);

	static const std::string xml_node_name;
	void load (const XMLNode&);

	GUIObjectState& operator= (const GUIObjectState& other);

	std::string get_string (const std::string& id, const std::string& prop_name, bool* empty = 0);

	template<typename T> void set (const std::string& id, const std::string& prop_name, const T& val) {
		StringPropertyMap::iterator i = _property_maps.find (id);
		
		if (i != _property_maps.end()) {
			i->second[prop_name] = val;
		} else {
			_property_maps[id] = PropertyMap();
			_property_maps[id][prop_name] = val;
		}
	}
  private:
	StringPropertyMap _property_maps;

	void clear_maps ();
};


#endif /* __gtk_ardour_gui_object_h__ */
