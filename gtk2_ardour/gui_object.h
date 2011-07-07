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
  public:
	GUIObjectState() {}
	~GUIObjectState();

	XMLNode& get_state () const;
	int set_state (const XMLNode&);

	static const std::string xml_node_name;
	void load (const XMLNode&);

	GUIObjectState& operator= (const GUIObjectState& other);

  private:
	typedef boost::variant<int64_t,std::string> Variant;
	typedef std::map<std::string,Variant> PropertyMap;
	typedef std::map<std::string,PropertyMap> StringPropertyMap;

	StringPropertyMap _property_maps;

	template<typename T> T get (const std::string& id, const std::string& prop_name, const T& type_determination_placeholder, bool* empty = 0) {
		StringPropertyMap::iterator i = _property_maps.find (id);
		
		if (i == _property_maps.end()) {
			if (empty) {
				*empty = true;
			}
			return T();
		}

		const PropertyMap& pmap (i->second);
		PropertyMap::const_iterator p = pmap.find (prop_name);

		if (p == pmap.end()) {
			return T();
		}

		return boost::get<T> (p->second);
	}

	void clear_maps ();

  public:
	int get_int (const std::string& id, const std::string& prop_name) {
		int i = 0;
		return get (id, prop_name, i);
	}

	std::string get_string (const std::string& id, const std::string& prop_name) {
		std::string s;
		return get (id, prop_name, s);
	}

	template<typename T> void set (const std::string& id, const std::string& prop_name, const T& val) {
		StringPropertyMap::iterator i = _property_maps.find (id);
		
		if (i != _property_maps.end()) {
			i->second[prop_name] = val;
			// std::cerr << id << " REset " << prop_name << " = [" << val << "]\n";
		} else {
			_property_maps[id] = PropertyMap();
			_property_maps[id][prop_name] = val;
			// std::cerr << id << " set " << prop_name << " = [" << val << "]\n";
		}
	}

};


#endif /* __gtk_ardour_gui_object_h__ */
