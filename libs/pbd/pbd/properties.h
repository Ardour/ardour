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
#include <sstream>
#include <list>
#include <set>
#include <iostream>

#include "pbd/libpbd_visibility.h"
#include "pbd/xml++.h"
#include "pbd/property_basics.h"
#include "pbd/property_list.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful.h"

namespace PBD {

/** Parent class for classes which represent a single scalar property in a Stateful object */
template<class T>
class /*LIBPBD_API*/ PropertyTemplate : public PropertyBase
{
public:
	PropertyTemplate (PropertyDescriptor<T> p, T const& v)
		: PropertyBase (p.property_id)
		, _have_old (false)
		, _current (v)
	{}

	PropertyTemplate (PropertyDescriptor<T> p, T const& o, T const& c)
		: PropertyBase (p.property_id)
		, _have_old (true)
		, _current (c)
		, _old (o)
	{}

	PropertyTemplate (PropertyDescriptor<T> p, PropertyTemplate<T> const & s)
		: PropertyBase (p.property_id)
		, _have_old (false)
		, _current (s._current)
	{}


	/* OPERATORS / ACCESSORS */

	T & operator=(T const& v) {
		set (v);
		return _current;
	}

	/* This will mean that, if fred and jim are both PropertyTemplates,
	 * fred = jim will result in fred taking on jim's current value,
	 * but NOT jim's property ID.
	 */
	PropertyTemplate<T> & operator= (PropertyTemplate<T> const & p) {
		set (p._current);
		return *this;
	}

	T & operator+=(T const& v) {
		set (_current + v);
		return _current;
	}

	bool operator==(const T& other) const {
		return _current == other;
	}

	bool operator!=(const T& other) const {
		return _current != other;
	}

	operator T const &() const {
		return _current;
	}

	T const& val () const {
		return _current;
	}


	/* MANAGEMENT OF Stateful State */
	
	bool set_value (XMLNode const & node) {

		XMLProperty const* p = node.property (property_name());

		if (p) {
			T const v = from_string (p->value ());

			if (v != _current) {
				set (v);
				return true;
			}
		}

		return false;
	}

	void get_value (XMLNode & node) const {
                node.add_property (property_name(), to_string (_current));
	}

	
	/* MANAGEMENT OF HISTORY */
	
	void clear_changes () {
		_have_old = false;
	}

	bool changed () const { return _have_old; }

	void invert () {
		T const tmp = _current;
		_current = _old;
		_old = tmp;
	}


	/* TRANSFERRING HISTORY TO / FROM A StatefulDiffCommand */
	
	void get_changes_as_xml (XMLNode* history_node) const {
		XMLNode* node = history_node->add_child (property_name());
                node->add_property ("from", to_string (_old));
                node->add_property ("to", to_string (_current));
	}

	void get_changes_as_properties (PropertyList& changes, Command *) const {
		if (this->_have_old) {
			changes.add (clone ());
		}
	}


	/* VARIOUS */

	void apply_changes (PropertyBase const * p) {
		T v = dynamic_cast<const PropertyTemplate<T>* > (p)->val ();
		if (v != _current) {
			set (v);
		}
	}

protected:

	void set (T const& v) {
		if (v != _current) {
			if (!_have_old) {
				_old = _current;
				_have_old = true;
			} else {
				if (v == _old) {
					/* value has been reset to the value
					   at the start of a history transaction,
					   before clear_changes() is called.
					   thus there is effectively no apparent
					   history for this property.
					*/
					_have_old = false;
				}
			}

			_current  = v;
		} 
	}

	virtual std::string to_string (T const& v) const             = 0;
	virtual T           from_string (std::string const& s) const = 0;

	bool _have_old;
	T _current;
	T _old;

private:
	/* disallow copy-construction; it's not obvious whether it should mean
	   a copy of just the value, or the value and property ID.
	*/
	PropertyTemplate (PropertyTemplate<T> const &);
};

template<class T> /*LIBPBD_API*/ 
std::ostream & operator<<(std::ostream& os, PropertyTemplate<T> const& s)
{
	return os << s.val ();
}

/** Representation of a single piece of scalar state in a Stateful; for use
 *  with types that can be written to / read from stringstreams.
 */
template<class T>
class /*LIBPBD_API*/ Property : public PropertyTemplate<T>
{
public:
	Property (PropertyDescriptor<T> q, T const& v)
		: PropertyTemplate<T> (q, v)
	{}

	Property (PropertyDescriptor<T> q, T const& o, T const& c)
		: PropertyTemplate<T> (q, o, c)
	{}

	Property (PropertyDescriptor<T> q, Property<T> const& v)
		: PropertyTemplate<T> (q, v)
	{}

	Property<T>* clone () const {
		return new Property<T> (this->property_id(), this->_old, this->_current);
	}
	
	Property<T>* clone_from_xml (const XMLNode& node) const {
		XMLNodeList const & children = node.children ();
		XMLNodeList::const_iterator i = children.begin();
		while (i != children.end() && (*i)->name() != this->property_name()) {
			++i;
		}

		if (i == children.end()) {
			return 0;
		}
		XMLProperty* from = (*i)->property ("from");
		XMLProperty* to = (*i)->property ("to");
				
		if (!from || !to) {
			return 0;
		}
			
		return new Property<T> (this->property_id(), from_string (from->value()), from_string (to->value ()));
	}

	T & operator=(T const& v) {
		this->set (v);
		return this->_current;
	}

private:
        friend class PropertyFactory;

	/* no copy-construction */
	Property (Property<T> const &);

	/* Note that we do not set a locale for the streams used
	 * in to_string() or from_string(), because we want the
	 * format to be portable across locales (i.e. C or
	 * POSIX). Also, there is the small matter of
	 * std::locale aborting on OS X if used with anything
	 * other than C or POSIX locales.
	 */
	virtual std::string to_string (T const& v) const {
		std::stringstream s;
		s.precision (12); // in case its floating point
		s << v;
		return s.str ();
	}

	virtual T from_string (std::string const& s) const {
		std::stringstream t (s);
		T                 v;
		t >> v;
		return v;
	}

};

/** Specialization, for std::string which is common and special (see to_string() and from_string()
 *  Using stringstream to read from a std::string is easy to get wrong because of whitespace
 *  separators, etc.
 */
template<>
class /*LIBPBD_API*/ Property<std::string> : public PropertyTemplate<std::string>
{
public:
	Property (PropertyDescriptor<std::string> d, std::string const & v)
		: PropertyTemplate<std::string> (d, v)
	{}

	Property (PropertyDescriptor<std::string> d, std::string const & o, std::string const & c)
		: PropertyTemplate<std::string> (d, o, c)
	{}
	
	Property<std::string>* clone () const {
		return new Property<std::string> (this->property_id(), _old, _current);
	}

	std::string & operator= (std::string const& v) {
		this->set (v);
		return this->_current;
	}

private:
	std::string to_string (std::string const& v) const {
		return v;
	}

	std::string from_string (std::string const& s) const {
		return s;
	}

	/* no copy-construction */
	Property (Property<std::string> const &);
};

template<class T>
class /*LIBPBD_API*/ EnumProperty : public Property<T>
{
public:
	EnumProperty (PropertyDescriptor<T> q, T const& v)
		: Property<T> (q, v)
	{}

	T & operator=(T const& v) {
		this->set (v);
		return this->_current;
	}

private:
	std::string to_string (T const & v) const {
		return enum_2_string (v);
	}

	T from_string (std::string const & s) const {
		return static_cast<T> (string_2_enum (s, this->_current));
	}

	/* no copy-construction */
	EnumProperty (EnumProperty const &);
};

/** A Property which holds a shared_ptr to a Stateful object,
 *  and handles undo using the somewhat inefficient approach
 *  of saving the complete XML state of its object before and
 *  after changes.  A sort of half-way house between the old
 *  complete-state undo system and the new difference-based
 *  one.
 */
template <class T>
class /*LIBPBD_API*/ SharedStatefulProperty : public PropertyBase
{
public:
	typedef boost::shared_ptr<T> Ptr;
	
	SharedStatefulProperty (PropertyID d, Ptr p)
		: PropertyBase (d)
		, _current (p)
	{
		
	}

	SharedStatefulProperty (PropertyID d, Ptr o, Ptr c)
		: PropertyBase (d)
		, _old (o)
		, _current (c)
	{
		
	}

	bool set_value (XMLNode const & node) {

		/* Look for our node */
		XMLNode* n = node.child (property_name ());
		if (!n) {
			return false;
		}

		/* And there should be one child which is the state of our T */
		XMLNodeList const & children = n->children ();
		if (children.size() != 1) {
			return false;
		}

		_current->set_state (*children.front (), Stateful::current_state_version);
		return true;
	}

	void get_value (XMLNode & node) const {
		XMLNode* n = node.add_child (property_name ());
		n->add_child_nocopy (_current->get_state ());
	}

	void clear_changes () {
		/* We are starting to change things, so _old gets set up
		   with the current state.
		*/
		_old.reset (new T (*_current.get()));
	}

	bool changed () const {
		/* Expensive, but, hey; this requires operator!= in
		   our T
		*/
		return (*_old != *_current);
	}

	void invert () {
		_current.swap (_old);
	}

	void get_changes_as_xml (XMLNode* history_node) const {
		/* We express the diff as before and after state, just
		   as MementoCommand does.
		*/
		XMLNode* p = history_node->add_child (property_name ());
		XMLNode* from = p->add_child ("from");
		from->add_child_nocopy (_old->get_state ());
		XMLNode* to = p->add_child ("to");
		to->add_child_nocopy (_current->get_state ());
	}

	void get_changes_as_properties (PropertyList& changes, Command *) const {
		if (changed ()) {
			changes.add (clone ());
		}
	}

	void apply_changes (PropertyBase const * p) {
		*_current = *(dynamic_cast<SharedStatefulProperty const *> (p))->val ();
	}

	Ptr val () const {
		return _current;
	}

	T* operator-> () const {
		return _current.operator-> ();
	}

	operator bool () const {
		return _current;
	}

protected:

	Ptr _old;
	Ptr _current;

private:

	/* No copy-construction nor assignment */
	SharedStatefulProperty (SharedStatefulProperty<T> const &);
	SharedStatefulProperty<T>& operator= (SharedStatefulProperty<T> const &);
};

} /* namespace PBD */

#include "pbd/property_list_impl.h"
#include "pbd/property_basics_impl.h"

#endif /* __pbd_properties_h__ */
