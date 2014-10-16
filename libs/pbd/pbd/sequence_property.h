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

#ifndef __libpbd_sequence_property_h__
#define __libpbd_sequence_property_h__

#include <iostream>

#include <set>
#include <list>

#include <boost/function.hpp>

#include "pbd/libpbd_visibility.h"
#include "pbd/convert.h"
#include "pbd/id.h"
#include "pbd/property_basics.h"
#include "pbd/property_list.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/error.h"

namespace PBD {

/** A base class for properties whose state is a container of other
 *  things.  Its behaviour is `specialised' for this purpose in that
 *  it holds state changes as additions to and removals from the
 *  container, which is more efficient than storing entire state after
 *  any change.
 */
template<typename Container>
class /*LIBPBD_API*/ SequenceProperty : public PropertyBase
{
  public:
        typedef std::set<typename Container::value_type> ChangeContainer;

	/** A record of changes made */
        struct ChangeRecord {

		void add (typename Container::value_type const & r) {
			typename ChangeContainer::iterator i = removed.find (r);
			if (i != removed.end()) {
				/* we're adding, and this thing has already been marked as removed, so
				   just remove it from the removed list
				*/
				removed.erase (r);
			} else {
				added.insert (r);
			}
		}

		void remove (typename Container::value_type const & r) {
			typename ChangeContainer::iterator i = added.find (r);
			if (i != added.end()) {
				/* we're removing, and this thing has already been marked as added, so
				   just remove it from the added list
				*/
				added.erase (i);
			} else {
				removed.insert (r);
			}
		}

		ChangeContainer added;
		ChangeContainer removed;
	};

	SequenceProperty (PropertyID id, const boost::function<void(const ChangeRecord&)>& update)
                : PropertyBase (id), _update_callback (update) {}

        void invert () {
		_changes.removed.swap (_changes.added);
        }

	void get_changes_as_xml (XMLNode* history_node) const {

                XMLNode* child = new XMLNode (PBD::capitalize (property_name()));
                history_node->add_child_nocopy (*child);
                
		/* record the change described in our change member */

		if (!_changes.added.empty()) {
			for (typename ChangeContainer::const_iterator i = _changes.added.begin(); i != _changes.added.end(); ++i) {
                                XMLNode* add_node = new XMLNode ("Add");
                                child->add_child_nocopy (*add_node);
				get_content_as_xml (*i, *add_node);
			}
		}
		if (!_changes.removed.empty()) {
			for (typename ChangeContainer::const_iterator i = _changes.removed.begin(); i != _changes.removed.end(); ++i) {
                                XMLNode* remove_node = new XMLNode ("Remove");
                                child->add_child_nocopy (*remove_node);
				get_content_as_xml (*i, *remove_node);
			}
		}
	}

	/** Get a representation of one of our items as XML.  The representation must be sufficient to
	 *  restore the item's state later; an ID is ok if someone else is storing the item state,
	 *  otherwise it needs to be the full state.  The supplied node is an \<Add\> or \<Remove\>
	 *  which this method can either add properties or children to.
	 */
	virtual void get_content_as_xml (typename ChangeContainer::value_type, XMLNode &) const = 0;

	bool set_value (XMLNode const &) {
		/* XXX: not used, but probably should be */
		assert (false);
		return false;
	}

	void get_value (XMLNode & node) const {
                for (typename Container::const_iterator i = _val.begin(); i != _val.end(); ++i) {
                        node.add_child_nocopy ((*i)->get_state ());
                } 
	}

	bool changed () const {
		return !_changes.added.empty() || !_changes.removed.empty();
	}
	
	void clear_changes () {
		_changes.added.clear ();
		_changes.removed.clear ();
	}

	void apply_changes (PropertyBase const * p) {
		const ChangeRecord& change (dynamic_cast<const SequenceProperty*> (p)->changes ());
		update (change);
	}

	/** Given a record of changes to this property, pass it to a callback that will
	 *  update the property in some appropriate way. 
	 *
	 *  This exists because simply using std::sequence methods to add/remove items
	 *  from the property is far too simplistic - the semantics of add/remove may
	 *  be much more complex than that.
	 */
	void update (const ChangeRecord& cr) {
		_update_callback (cr);
	}

	void get_changes_as_properties (PBD::PropertyList& changes, Command* cmd) const {
		if (!changed ()) {
			return;
		}
		
		/* Create a property with just the changes and not the actual values */
		SequenceProperty<Container>* a = create ();
		a->_changes = _changes;
		changes.add (a);
		
		if (cmd) {
			/* whenever one of the items emits DropReferences, make sure
			   that the Destructible we've been told to notify hears about
			   it. the Destructible is likely to be the Command being built
			   with this diff().
			*/
                        
			for (typename ChangeContainer::const_iterator i = a->changes().added.begin(); i != a->changes().added.end(); ++i) {
				(*i)->DropReferences.connect_same_thread (*cmd, boost::bind (&Destructible::drop_references, cmd));
			}
		}
        }

	SequenceProperty<Container>* clone_from_xml (XMLNode const & node) const {

		XMLNodeList const children = node.children ();

		/* find the node for this property name */
		
		std::string const c = capitalize (property_name ());
		XMLNodeList::const_iterator i = children.begin();
		while (i != children.end() && (*i)->name() != c) {
			++i;
		}

		if (i == children.end()) {
			return 0;
		}

		/* create a property with the changes */
		
		SequenceProperty<Container>* p = create ();

		XMLNodeList const & grandchildren = (*i)->children ();
		for (XMLNodeList::const_iterator j = grandchildren.begin(); j != grandchildren.end(); ++j) {

			typename Container::value_type v = get_content_from_xml (**j);

			if (!v) {
				warning << "undo transaction references an unknown object" << endmsg;
			} else if ((*j)->name() == "Add") {
				p->_changes.added.insert (v);
			} else if ((*j)->name() == "Remove") {
				p->_changes.removed.insert (v);
			}
		}

		return p;
        }

	/** Given an \<Add\> or \<Remove\> node as passed into get_content_to_xml, obtain an item */
	virtual typename Container::value_type get_content_from_xml (XMLNode const & node) const = 0;

	void clear_owned_changes () {
		for (typename Container::iterator i = begin(); i != end(); ++i) {
			(*i)->clear_changes ();
		}
	}

	void rdiff (std::vector<Command*>& cmds) const {
		for (typename Container::const_iterator i = begin(); i != end(); ++i) {
			if ((*i)->changed ()) {
				StatefulDiffCommand* sdc = new StatefulDiffCommand (*i);
				cmds.push_back (sdc);
			}
		}
	}

        Container rlist() const { return _val; }

	/* Wrap salient methods of Sequence
	 */

	typename Container::iterator begin() { return _val.begin(); }
	typename Container::iterator end() { return _val.end(); }
	typename Container::const_iterator begin() const { return _val.begin(); }
	typename Container::const_iterator end() const { return _val.end(); }

	typename Container::reverse_iterator rbegin() { return _val.rbegin(); }
	typename Container::reverse_iterator rend() { return _val.rend(); }
	typename Container::const_reverse_iterator rbegin() const { return _val.rbegin(); }
	typename Container::const_reverse_iterator rend() const { return _val.rend(); }

	typename Container::iterator insert (typename Container::iterator i, const typename Container::value_type& v) {
		_changes.add (v);
		return _val.insert (i, v);
	}

	typename Container::iterator erase (typename Container::iterator i) {
		if (i != _val.end()) {
			_changes.remove (*i);
		}
		return _val.erase (i);
	}

	typename Container::iterator erase (typename Container::iterator f, typename Container::iterator l) {
		for (typename Container::const_iterator i = f; i != l; ++i) {
			_changes.remove (*i);
		}
		return _val.erase (f, l);
	}

	void remove (const typename Container::value_type& v) {
		_changes.remove (v);
		_val.remove (v);
	}

	void push_back (const typename Container::value_type& v) {
		_changes.add (v);
		_val.push_back (v);
	}

	void push_front (const typename Container::value_type& v) {
		_changes.add (v);
		_val.push_front (v);
	}

	void pop_front () {
                if (!_val.empty()) {
                        _changes.remove (front());
                }
		_val.pop_front ();
	}

	void pop_back () {
                if (!_val.empty()) {
                        _changes.remove (back());
                }
		_val.pop_back ();
	}

	void clear () {
		for (typename Container::iterator i = _val.begin(); i != _val.end(); ++i) {
			_changes.remove (*i);
		}
		_val.clear ();
	}
	
	typename Container::size_type size() const { 
		return _val.size();
	}

	bool empty() const { 
		return _val.empty();
	}

	Container& operator= (const Container& other) {
		for (typename Container::const_iterator i = _val.begin(); i != _val.end(); ++i) {
			_changes.remove (*i);
		}
		for (typename Container::const_iterator i = other.begin(); i != other.end(); ++i) {
			_changes.add (*i);
		}
		return _val = other;
	}

	typename Container::reference front() { 
		return _val.front ();
	}

	typename Container::const_reference front() const { 
		return _val.front ();
	}

	typename Container::reference back() { 
		return _val.back ();
	}

	typename Container::const_reference back() const { 
		return _val.back ();
	}

	void sort() { 
		_val.sort ();
	}

	template<class BinaryPredicate> void sort(BinaryPredicate comp) {
		_val.sort (comp);
	}
        
        const ChangeRecord& changes () const { return _changes; }

protected:

	/* copy construction only by subclasses */
	SequenceProperty (SequenceProperty<Container> const & p)
		: PropertyBase (p)
		, _val (p._val)
		, _changes (p._changes)
		, _update_callback (p._update_callback)
	{}
	
	Container _val; ///< our actual container of things
	ChangeRecord _changes; ///< changes to the container (adds/removes) that have happened since clear_changes() was last called
	boost::function<void(const ChangeRecord&)> _update_callback;

private:	
	virtual SequenceProperty<Container>* create () const = 0;
};

}

#endif /* __libpbd_sequence_property_h__ */
