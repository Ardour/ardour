#ifndef __libpbd_sequence_property_h__
#define __libpbd_sequence_property_h__

#include <iostream>

#include <set>
#include <list>

#include <boost/function.hpp>

#include "pbd/id.h"
#include "pbd/property_basics.h"

#include "i18n.h"

namespace PBD {
template<typename Container>
class SequenceProperty : public PropertyBase
{
  public:
        typedef std::set<typename Container::value_type> ChangeContainer;
	
        struct ChangeRecord { 
	    ChangeContainer added;
	    ChangeContainer removed;
	};

	SequenceProperty (PropertyID id, const boost::function<void(const ChangeRecord&)>& update)
                : PropertyBase (id), _update_callback (update) {}
        
	virtual typename Container::value_type lookup_id (const PBD::ID&) = 0;

        void invert_changes () {

                /* reverse the adds/removes so that this property's change member
                   correctly describes how to undo the changes it currently
                   reflects. A derived instance of this type of property will
                   create a pdiff() pair by copying the property twice, and
                   calling this method on the "before" item of the pair.
                */

                _change.removed.swap (_change.added);
        }

	void add_history_state (XMLNode* history_node) const {
                
                /* XXX need to capitalize property name */
                XMLNode* child = new XMLNode (property_name());
                history_node->add_child_nocopy (*child);
                
		/* record the change described in our change member */

		if (!_change.added.empty()) {
			for (typename ChangeContainer::iterator i = _change.added.begin(); i != _change.added.end(); ++i) {
                                XMLNode* add_node = new XMLNode (X_("Add"));
                                child->add_child_nocopy (*add_node);
                                add_node->add_property (X_("id"), (*i)->id().to_s());
			}
		}
		if (!_change.removed.empty()) {
			for (typename ChangeContainer::iterator i = _change.removed.begin(); i != _change.removed.end(); ++i) {
                                XMLNode* remove_node = new XMLNode (X_("Remove"));
                                child->add_child_nocopy (*remove_node);
                                remove_node->add_property (X_("id"), (*i)->id().to_s());
			}
		}
	}

	bool set_state_from_owner_state (XMLNode const& owner_state) {

		XMLProperty const* n = owner_state.property (X_("name"));

                if (!n) {
                        return false;
                }

                assert (g_quark_from_string (n->value().c_str()) == property_id());

		const XMLNodeList& children = owner_state.children();

		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {

			if ((*c)->name() == X_("Added")) {
				const XMLNodeList& grandchildren = (*c)->children();
				for (XMLNodeList::const_iterator gc = grandchildren.begin(); gc != grandchildren.end(); ++gc) {
					const XMLProperty* prop = (*gc)->property (X_("id"));
					if (prop) {
						typename Container::value_type v = lookup_id (PBD::ID (prop->value()));
						if (v) {
							_change.added.insert (v);
						}
					}
				}
			} else if ((*c)->name() == X_("Removed")) {
				const XMLNodeList& grandchildren = (*c)->children();
				for (XMLNodeList::const_iterator gc = grandchildren.begin(); gc != grandchildren.end(); ++gc) {
					const XMLProperty* prop = (*gc)->property (X_("id"));
					if (prop) {
						typename Container::value_type v = lookup_id (PBD::ID (prop->value()));
						if (v) {
							_change.removed.insert (v);
						}
					}
				}
			}
		}

		return true;
	}

	void add_state_to_owner_state (XMLNode& owner_state_node) const {
                for (typename Container::const_iterator i = _val.begin(); i != _val.end(); ++i) {
                        owner_state_node.add_child_nocopy ((*i)->get_state ());
                } 
	}
	
	void clear_history () {
		PropertyBase::clear_history();
		_change.added.clear ();
		_change.removed.clear ();
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
                _have_old = true;
		_change.added.insert (v);
		return _val.insert (i, v);
	}

	typename Container::iterator erase (typename Container::iterator i) {
		if (i != _val.end()) {
                        _have_old = true;
			_change.removed.insert (*i);
		}
		return _val.erase (i);
	}

	typename Container::iterator erase (typename Container::iterator f, typename Container::iterator l) {
                _have_old = true;
		for (typename Container::const_iterator i = f; i != l; ++i) {
			_change.removed.insert(*i);
		}
		return _val.erase (f, l);
	}

	void push_back (const typename Container::value_type& v) {
                _have_old = true;
		_change.added.insert (v);
		_val.push_back (v);
	}

	void push_front (const typename Container::value_type& v) {
                _have_old = true;
		_change.added.insert (v);
		_val.push_front (v);
	}

	void pop_front () {
                if (!_val.empty()) {
                        _have_old = true;
                        _change.removed.insert (front());
                }
		_val.pop_front ();
	}

	void pop_back () {
                if (!_val.empty()) {
                        _have_old = true;
                        _change.removed.insert (front());
                }
		_val.pop_back ();
	}

	void clear () {
                _have_old = true;
		_change.removed.insert (_val.begin(), _val.end());
		_val.clear ();
	}
	
	typename Container::size_type size() const { 
		return _val.size();
	}

	bool empty() const { 
		return _val.empty();
	}

	Container& operator= (const Container& other) {
                _have_old = true;
                _change.removed.insert (_val.begin(), _val.end());
                _change.added.insert (other.begin(), other.end());
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
        
        const ChangeRecord& change() const { return _change; }

        /* for use in building up a SequenceProperty from a serialized
           version on disk.
        */

        void record_addition (typename Container::value_type v) {
                _change.added.insert (v);
        }
        void record_removal (typename Container::value_type v) {
                _change.added.erase (v);
        }

  protected:
	Container _val;
	ChangeRecord _change;
	boost::function<void(const ChangeRecord&)> _update_callback;

        /** Load serialized change history.
         * @return true if loading succeeded, false otherwise
         */

        bool load_history_state (const XMLNode& history_node) {

                const XMLNodeList& children (history_node.children());

                for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
                        const XMLProperty* prop = (*i)->property ("id");
                        if (prop) {
                                PBD::ID id (prop->value());
                                typename Container::value_type v = lookup_id (id);
                                if (!v) {
                                        std::cerr << "No such item, ID = " << id.to_s() << " (from " << prop->value() << ")\n";
                                        return false;
                                }
                                if ((*i)->name() == "Add") {
                                        _change.added.insert (v);
                                } else if ((*i)->name() == "Remove") {
                                        _change.removed.insert (v);
                                }
                        }
                }

                return true;
        }
};

}

#endif /* __libpbd_sequence_property_h__ */
