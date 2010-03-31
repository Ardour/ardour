/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __libpbd_property_basics_h__
#define __libpbd_property_basics_h__

#include <glib.h>
#include <set>

#include "pbd/xml++.h"

namespace PBD {

class PropertyList;
typedef GQuark PropertyID;

template<typename T>
struct PropertyDescriptor {
    PropertyDescriptor () : property_id (0) {}
    PropertyDescriptor (PropertyID pid) : property_id (pid) {}

    PropertyID property_id;
    typedef T value_type;
};

class PropertyChange : public std::set<PropertyID>
{
public:
	PropertyChange() {}

	template<typename T> PropertyChange(PropertyDescriptor<T> p);

	PropertyChange(const PropertyChange& other) : std::set<PropertyID> (other) {}

	PropertyChange operator=(const PropertyChange& other) {
		clear ();
		insert (other.begin (), other.end ());
		return *this;
	}

	template<typename T> PropertyChange operator=(PropertyDescriptor<T> p);
	template<typename T> bool contains (PropertyDescriptor<T> p) const;

	bool contains (const PropertyChange& other) const {
		for (const_iterator x = other.begin (); x != other.end (); ++x) {
			if (find (*x) != end ()) {
				return true;
			}
		}
		return false;
	}

	void add (PropertyID id)               { insert (id); }
	void add (const PropertyChange& other) { insert (other.begin (), other.end ()); }
	template<typename T> void add (PropertyDescriptor<T> p);
};

/** Base (non template) part of Property */
class PropertyBase
{
public:
	PropertyBase (PropertyID pid)
		: _property_id (pid)
	{}

	/** Forget about any old value for this state */
	virtual void clear_history () = 0;

	virtual void add_history_state (XMLNode*)        const = 0;
	virtual void diff (PropertyList&, PropertyList&) const = 0;
        
        virtual PropertyBase* maybe_clone_self_if_found_in_history_node (const XMLNode&) const { return 0; }

	virtual bool set_state_from_owner_state (XMLNode const&) = 0;
	virtual void add_state_to_owner_state (XMLNode&) const   = 0;
	virtual bool changed() const = 0;

	const gchar*property_name () const { return g_quark_to_string (_property_id); }
	PropertyID  property_id () const   { return _property_id; }

	bool operator==(PropertyID pid) const {
		return _property_id == pid;
	}

protected:
	PropertyID _property_id;
};

}

#endif /* __libpbd_property_basics_h__ */
