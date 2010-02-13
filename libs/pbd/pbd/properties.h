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

#ifndef __pbd_properties_h__
#define __pbd_properties_h__

#include <string>
#include <list>
#include <glib/glib.h>

class XMLNode;

namespace PBD {

enum PropertyChange {
	range_guarantee = ~0
};

PropertyChange new_change ();

/** Base (non template) part of Property */	
class PropertyBase
{
public:
	PropertyBase (GQuark quark, PropertyChange c)
		: _have_old (false)
		, _property_quark (q)
		, _change (c)
	{

	}

	/** Forget about any old value for this state */
	void clear_history () {
		_have_old = false;
	}

	virtual void diff (XMLNode *, XMLNode *) const = 0;
	virtual PropertyChange set_state (XMLNode const &) = 0;
	virtual void add_state (XMLNode &) const = 0;

	std::string property_name() const { return g_quark_to_string (_property_quark); }
	PropertyChange change() const { return _change; }

	bool operator== (GQuark q) const {
		return _property_quark == q;
	}

protected:
	bool _have_old;
	GQuark _property_quark;
	PropertyChange _change;
};

/** Parent class for classes which represent a single property in a Stateful object */
template <class T>
class PropertyTemplate : public PropertyBase
{
public:
	PropertyTemplate (GQuark q, PropertyChange c, T const & v)
		: PropertyBase (q, c)
		, _current (v)
	{

	}

	PropertyTemplate<T> & operator= (PropertyTemplate<T> const & s) {
		/* XXX: isn't there a nicer place to do this? */
		_have_old = s._have_old;
		_property_quark = s._property_quark;
		_change = s._change;
		
		_current = s._current;
		_old = s._old;
		return *this;
	}

	T & operator= (T const & v) {
		set (v);
		return _current;
	}

	T & operator+= (T const & v) {
		set (_current + v);
		return _current;
	}
	
	bool operator== (const T& other) const {
		return _current == other;
	}

	bool operator!= (const T& other) const {
		return _current != other;
	}

	operator T const & () const {
		return _current;
	}

	T const & val () const {
		return _current;
	}

	void diff (XMLNode* old, XMLNode* current) const {
		if (_have_old) {
			old->add_property (g_quark_to_string (_property_quark), to_string (_old));
			current->add_property (g_quark_to_string (_property_quark), to_string (_current));
		}
	}

	/** Try to set state from the property of an XML node.
	 *  @param node XML node.
	 *  @return PropertyChange effected, or 0.
	 */
	PropertyChange set_state (XMLNode const & node) {
		XMLProperty const * p = node.property (g_quark_to_string (_property_quark));

		if (p) {
			T const v = from_string (p->value ());

			if (v == _current) {
				return PropertyChange (0);
			}

			set (v);
			return _change;
		}

		return PropertyChange (0);
	}

	void add_state (XMLNode & node) const {
		node.add_property (g_quark_to_string (_property_quark), to_string (_current));
	}

protected:
	void set (T const & v) {
		_old = _current;
		_have_old = true;
		_current = v;
	}

	virtual std::string to_string (T const & v) const = 0;
	virtual T from_string (std::string const & s) const = 0;
		
	T _current;
	T _old;
};

template<class T>	
std::ostream& operator<< (std::ostream& os, PropertyTemplate<T> const & s)
{
	return os << s.val();
}

/** Representation of a single piece of state in a Stateful; for use
 *  with types that can be written to / read from stringstreams.
 */
template <class T>
class Property : public PropertyTemplate<T>
{
public:
	Property (GQuark q, PropertyChange c, T const & v)
		: PropertyTemplate<T> (q, c, v)
	{

	}
	
	T & operator= (T const & v) {
		this->set (v);
		return this->_current;
	}
	
private:	
	std::string to_string (T const & v) const {
		// XXX LocaleGuard
		std::stringstream s;
		s.precision (12); // in case its floating point
		s << v;
		return s.str ();
	}

	T from_string (std::string const & s) const {
		// XXX LocaleGuard	
		std::stringstream t (s);
		T v;
		t.precision (12); // in case its floating point
		t >> v;
		return v;
	}
};

} /* namespace PBD */

#endif /* __pbd_properties_h__ */
